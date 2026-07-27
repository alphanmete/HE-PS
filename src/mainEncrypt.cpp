#include "VectorUtils.hpp"

#ifdef USE_SEAL
#ifdef USE_CKKS
#include "HEAggregatorCKKS.hpp"
#else
#include "HEAggregator.hpp"
#endif
#endif

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

static volatile std::size_t g_size_sink = 0;
static volatile double g_double_sink = 0.0;

class WallTimer {
public:
    void start() {
        start_ = Clock::now();
    }

    long long stop_us() const {
        const auto end = Clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(end - start_).count();
    }

private:
    using Clock = std::chrono::steady_clock;
    Clock::time_point start_;
};

static bool parse_int_arg(const std::string& text, int& value) {
    if (text.empty()) {
        return false;
    }

    char* end = nullptr;
    errno = 0;
    const long parsed = std::strtol(text.c_str(), &end, 10);

    if (errno != 0 || end == text.c_str() || *end != '\0') {
        return false;
    }

    if (parsed < INT_MIN || parsed > INT_MAX) {
        return false;
    }

    value = static_cast<int>(parsed);
    return true;
}

static void flush_cpu_cache(std::size_t cache_mb) {
    const std::size_t size = cache_mb * 1024ULL * 1024ULL;
    static std::vector<char> buffer;

    if (buffer.size() != size) {
        buffer.assign(size, 1);
    }

    volatile std::uint64_t sum = 0;

    for (std::size_t i = 0; i < buffer.size(); i += 64) {
        sum += static_cast<unsigned char>(buffer[i]);
    }

    g_size_sink += static_cast<std::size_t>(sum);
}

static void maybe_flush(bool cold_cache, std::size_t cache_mb) {
    if (cold_cache) {
        flush_cpu_cache(cache_mb);
    }
}

static std::string scheme_name() {
#ifdef USE_SEAL
#ifdef USE_CKKS
    return "CKKS";
#else
    return "BFV";
#endif
#else
    return "PLAIN";
#endif
}

static Profile make_representative_profile(std::size_t profile_size) {
    Profile profile(profile_size, 0.0);

    for (std::size_t i = 0; i < profile_size; ++i) {
        const double daily_shape = static_cast<double>(i % 24);
        const double block_shape = static_cast<double>((i / 4) % 6);

        profile[i] = 500.0 + 12.5 * daily_shape + 7.0 * block_shape;
    }

    return profile;
}

#ifdef USE_SEAL
#ifdef USE_CKKS
using ActiveHE = HEAggregatorCKKS;
#else
using ActiveHE = HEAggregator;
#endif
#endif

int main(int argc, char* argv[]) {
#ifndef USE_SEAL
    std::cerr << "This benchmark is only meaningful for BFV/CKKS builds with USE_SEAL=ON.\n";
    std::cerr << "Build BFV:  cmake -S . -B cmake-build-bfv -DCMAKE_BUILD_TYPE=Release -DUSE_SEAL=ON\n";
    std::cerr << "Build CKKS: cmake -S . -B cmake-build-ckks -DCMAKE_BUILD_TYPE=Release -DUSE_SEAL=ON -DUSE_CKKS=ON\n";
    return 1;
#else
    int repetitions = 1000;
    bool cold_cache = true;
    int cache_mb_int = 512;

    int numeric_seen = 0;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "cold") {
            cold_cache = true;
            continue;
        }

        if (arg == "warm") {
            cold_cache = false;
            continue;
        }

        int parsed = 0;
        if (!parse_int_arg(arg, parsed)) {
            std::cerr << "Unknown argument: " << arg << "\n";
            std::cerr << "Usage: " << argv[0] << " [repetitions] [warm|cold] [cache_mb]\n";
            return 1;
        }

        if (numeric_seen == 0) {
            repetitions = parsed;
        } else if (numeric_seen == 1) {
            cache_mb_int = parsed;
        } else {
            std::cerr << "Too many numeric arguments.\n";
            std::cerr << "Usage: " << argv[0] << " [repetitions] [warm|cold] [cache_mb]\n";
            return 1;
        }

        ++numeric_seen;
    }

    if (repetitions <= 0) {
        std::cerr << "repetitions must be positive.\n";
        return 1;
    }

    if (cache_mb_int <= 0) {
        std::cerr << "cache_mb must be positive.\n";
        return 1;
    }

    constexpr std::size_t profile_size = 96;
    const std::size_t cache_mb = static_cast<std::size_t>(cache_mb_int);
    const std::string scheme = scheme_name();
    const std::string cache_mode = cold_cache ? "cold" : "warm";

    Profile profile = make_representative_profile(profile_size);

    std::cerr << "[INFO] A encryption-only benchmark\n";
    std::cerr << "[INFO] Scheme: " << scheme << "\n";
    std::cerr << "[INFO] Repetitions: " << repetitions << "\n";
    std::cerr << "[INFO] Cache mode: " << cache_mode << "\n";
    std::cerr << "[INFO] Cache flush size: " << cache_mb << " MB\n";
    std::cerr << "[INFO] Profile size: " << profile_size << "\n";
    std::cerr << "[INFO] Timed operation: encode + encrypt one 96-value plaintext profile.\n";
    std::cerr << "[INFO] HE setup/key generation is excluded from timing.\n";
    std::cerr << "[INFO] Aggregation, decryption, and Profile Steering are excluded.\n\n";

    ActiveHE he(profile_size);

    // Untimed warm-up to avoid measuring one-time lazy setup effects.
    for (int i = 0; i < 5; ++i) {
        seal::Ciphertext warmup = he.encrypt_profile(profile);
        g_size_sink += static_cast<std::size_t>(warmup.save_size());
    }

    WallTimer timer;

    long long total_us = 0;
    long long min_us = std::numeric_limits<long long>::max();
    long long max_us = 0;
    std::size_t cipher_bytes = 0;

    for (int r = 0; r < repetitions; ++r) {
        maybe_flush(cold_cache, cache_mb);

        timer.start();
        seal::Ciphertext encrypted = he.encrypt_profile(profile);
        const long long elapsed_us = timer.stop_us();

        total_us += elapsed_us;
        min_us = std::min(min_us, elapsed_us);
        max_us = std::max(max_us, elapsed_us);

        cipher_bytes = static_cast<std::size_t>(encrypted.save_size());
        g_size_sink += cipher_bytes;

        if (!profile.empty()) {
            g_double_sink += profile[0];
        }
    }

    const double avg_us =
        static_cast<double>(total_us) / static_cast<double>(repetitions);

    std::cerr << "========================================\n";
    std::cerr << "A Encryption-Only Benchmark Report\n";
    std::cerr << "========================================\n";
    std::cerr << "[TIMER] A_encrypt_profile"
              << " | total: " << total_us << " us"
              << " | repetitions: " << repetitions
              << " | avg: " << avg_us << " us"
              << " | min: " << min_us << " us"
              << " | max: " << max_us << " us"
              << " | cipher bytes: " << cipher_bytes
              << "\n";
    std::cerr << "[SUMMARY] size sink: " << g_size_sink
              << " double sink: " << g_double_sink << "\n";

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "scheme,metric,repetitions,cache_mode,cache_mb,profile_size,total_us,avg_us,min_us,max_us,cipher_bytes\n";
    std::cout << scheme << ","
              << "A_encrypt_profile" << ","
              << repetitions << ","
              << cache_mode << ","
              << cache_mb << ","
              << profile_size << ","
              << total_us << ","
              << avg_us << ","
              << min_us << ","
              << max_us << ","
              << cipher_bytes << "\n";

    return 0;
#endif
}