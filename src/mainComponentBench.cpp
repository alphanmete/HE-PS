#include "Household.hpp"
#include "Devices.hpp"
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
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

static volatile double g_double_sink = 0.0;
static volatile std::size_t g_size_sink = 0;

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

struct Metric {
    std::string name;
    long long total_us = 0;
    long long count = 0;
    std::size_t cipher_bytes = 0;

    void add(long long us, long long n = 1) {
        total_us += us;
        count += n;
    }

    double avg_us() const {
        if (count == 0) {
            return 0.0;
        }
        return static_cast<double>(total_us) / static_cast<double>(count);
    }
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

static void flush_cpu_cache() {
    constexpr std::size_t SIZE = 512ULL * 1024ULL * 1024ULL;
    static std::vector<char> buffer(SIZE);

    volatile std::uint64_t sum = 0;
    for (std::size_t i = 0; i < SIZE; i += 64) {
        sum += static_cast<unsigned char>(buffer[i]);
    }

    g_size_sink += static_cast<std::size_t>(sum);
}

static void maybe_flush(bool cold_cache) {
    if (cold_cache) {
        flush_cpu_cache();
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

static Profile scale_profile_local(const Profile& profile, double factor) {
    Profile scaled(profile.size(), 0.0);
    for (std::size_t i = 0; i < profile.size(); ++i) {
        scaled[i] = profile[i] * factor;
    }
    return scaled;
}

static std::unique_ptr<Household> create_household(
    int id,
    std::mt19937& rng,
    int intervals
) {
    std::vector<std::unique_ptr<Device>> devices;

    devices.push_back(std::make_unique<Load>(rng));
    devices.push_back(std::make_unique<Battery>());
    devices.push_back(std::make_unique<ElectricVehicle>(rng, intervals));
    devices.push_back(std::make_unique<HeatPump>(rng));

    return std::make_unique<Household>(
        "Household_" + std::to_string(id),
        std::move(devices)
    );
}

static void print_metric_csv(
    const std::string& scheme,
    const Metric& metric,
    int households,
    int simulations,
    const std::string& cache_mode
) {
    std::cout << scheme << ","
              << metric.name << ","
              << households << ","
              << simulations << ","
              << cache_mode << ","
              << metric.total_us << ","
              << metric.avg_us() << ","
              << metric.count << ","
              << metric.cipher_bytes << "\n";
}

static void print_metric_human(const Metric& metric) {
    std::cerr << "[TIMER] " << metric.name
              << " | total: " << metric.total_us << " us"
              << " | count: " << metric.count
              << " | avg: " << metric.avg_us() << " us";

    if (metric.cipher_bytes > 0) {
        std::cerr << " | cipher bytes: " << metric.cipher_bytes;
    }

    std::cerr << "\n";
}

#ifdef USE_SEAL
#ifdef USE_CKKS
using ActiveHE = HEAggregatorCKKS;
#else
using ActiveHE = HEAggregator;
#endif
#endif

int main(int argc, char* argv[]) {
    constexpr int intervals = 96;
    constexpr double e_min = 0.001;
    constexpr int max_iters = 100;
    constexpr double beta = 2.0;

    int household_count = 10;
    int simulation_count = 1;
    bool cold_cache = true;
    bool quiet = true;

    int numeric_seen = 0;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "warm") {
            cold_cache = false;
            continue;
        }
        if (arg == "cold") {
            cold_cache = true;
            continue;
        }
        if (arg == "verbose") {
            quiet = false;
            continue;
        }
        if (arg == "quiet") {
            quiet = true;
            continue;
        }

        int parsed = 0;
        if (!parse_int_arg(arg, parsed)) {
            std::cerr << "Unknown argument: " << arg << "\n";
            std::cerr << "Usage: " << argv[0] << " [households] [simulations] [warm|cold] [quiet|verbose]\n";
            return 1;
        }

        if (numeric_seen == 0) {
            household_count = parsed;
        } else if (numeric_seen == 1) {
            simulation_count = parsed;
        } else {
            std::cerr << "Too many numeric arguments.\n";
            std::cerr << "Usage: " << argv[0] << " [households] [simulations] [warm|cold] [quiet|verbose]\n";
            return 1;
        }
        ++numeric_seen;
    }

    if (household_count <= 0 || simulation_count <= 0) {
        std::cerr << "households and simulations must both be positive.\n";
        return 1;
    }

    const std::string scheme = scheme_name();
    const std::string cache_mode = cold_cache ? "cold" : "warm";

    Metric h_initial_device_planning {"H_initial_device_planning"};
    Metric h_encrypt_x {"H_encrypt_X_i"};
    Metric h_plan_compute {"H_plan_compute"};
    Metric h_accept_compute {"H_accept_compute"};
    Metric h_encrypt_delta {"H_encrypt_delta_i"};

    Metric b_init_x_from_first {"B_init_X_from_first_ciphertext"};
    Metric b_add_x_remaining {"B_add_X_remaining"};
    Metric b_init_delta_from_first {"B_init_delta_from_first_ciphertext"};
    Metric b_add_delta_remaining {"B_add_delta_remaining"};

    Metric c_decrypt_x {"C_decrypt_X"};
    Metric c_decrypt_delta {"C_decrypt_delta"};
    Metric c_control_logic {"C_control_logic_until_convergence"};

    long long total_iterations = 0;
    long long total_accepted_deltas = 0;
    double checksum = 0.0;

    WallTimer timer;

    std::cerr << "[INFO] H/B/C full-convergence benchmark\n";
    std::cerr << "[INFO] Scheme: " << scheme << "\n";
    std::cerr << "[INFO] Households: " << household_count << "\n";
    std::cerr << "[INFO] Simulations: " << simulation_count << "\n";
    std::cerr << "[INFO] Cache mode: " << cache_mode << "\n";
    std::cerr << "[INFO] Cold mode flushes cache once before each simulation, matching full-run benchmark style.\n";
    std::cerr << "[INFO] H timers measure household planning/acceptance and, for HE builds, household encryption.\n";
    std::cerr << "[INFO] B timers measure aggregation by initializing from the first submitted vector/ciphertext and adding only remaining submissions.\n";
    std::cerr << "[INFO] C timers measure coordinator decrypt and control logic only.\n";
    std::cerr << "[INFO] A, the standalone PC/Raspberry Pi encryption-only benchmark, should still be run separately.\n\n";

#ifdef USE_SEAL
    ActiveHE he(static_cast<std::size_t>(intervals));
#endif

    for (int simulation = 0; simulation < simulation_count; ++simulation) {
        maybe_flush(cold_cache);

        Profile desired(static_cast<std::size_t>(intervals), 0.0);
        Profile zero(static_cast<std::size_t>(intervals), 0.0);

        std::mt19937 rng(42 + simulation);
        std::vector<std::unique_ptr<Household>> households;
        households.reserve(static_cast<std::size_t>(household_count));

        for (int i = 0; i < household_count; ++i) {
            households.push_back(create_household(i, rng, intervals));
        }

        std::vector<Profile> initial_profiles;
        initial_profiles.reserve(static_cast<std::size_t>(household_count));

        long long init_us = 0;
        for (auto& household : households) {
            timer.start();
            initial_profiles.push_back(household->init(zero));
            init_us += timer.stop_us();
        }
        h_initial_device_planning.add(init_us, household_count);

        Profile x(static_cast<std::size_t>(intervals), 0.0);

#ifdef USE_SEAL
        std::vector<seal::Ciphertext> encrypted_initial_profiles;
        encrypted_initial_profiles.reserve(static_cast<std::size_t>(household_count));

        long long encrypt_x_us = 0;
        for (const Profile& profile : initial_profiles) {
            timer.start();
            seal::Ciphertext encrypted_profile = he.encrypt_profile(profile);
            encrypt_x_us += timer.stop_us();
            h_encrypt_x.cipher_bytes = static_cast<std::size_t>(encrypted_profile.save_size());
            encrypted_initial_profiles.push_back(std::move(encrypted_profile));
        }
        h_encrypt_x.add(encrypt_x_us, household_count);

        timer.start();
        seal::Ciphertext encrypted_x_total = encrypted_initial_profiles.front();
        b_init_x_from_first.add(timer.stop_us(), 1);
        b_init_x_from_first.cipher_bytes = static_cast<std::size_t>(encrypted_x_total.save_size());

        long long add_x_us = 0;
        for (std::size_t i = 1; i < encrypted_initial_profiles.size(); ++i) {
            timer.start();
            he.add_inplace(encrypted_x_total, encrypted_initial_profiles[i]);
            add_x_us += timer.stop_us();
        }
        b_add_x_remaining.add(add_x_us, std::max(0, household_count - 1));
        b_add_x_remaining.cipher_bytes = static_cast<std::size_t>(encrypted_x_total.save_size());

        timer.start();
        x = he.decrypt_profile(encrypted_x_total);
        c_decrypt_x.add(timer.stop_us(), 1);
#else
        timer.start();
        x = initial_profiles.front();
        b_init_x_from_first.add(timer.stop_us(), 1);

        timer.start();
        for (std::size_t i = 1; i < initial_profiles.size(); ++i) {
            x = add_profiles(x, initial_profiles[i]);
        }
        b_add_x_remaining.add(timer.stop_us(), std::max(0, household_count - 1));
#endif

        int mu = household_count;

        for (int iter = 0; iter < max_iters; ++iter) {
            if (mu < 1) {
                mu = 1;
            }

            ++total_iterations;

            long long c_iter_us = 0;

            timer.start();
            Profile d = sub_profiles(x, desired);
            Profile divided_d = scale_profile_local(
                d,
                1.0 / static_cast<double>(mu)
            );
            c_iter_us += timer.stop_us();

            std::vector<std::pair<double, Household*>> candidates;
            candidates.reserve(households.size());

            long long plan_us = 0;
            for (auto& household : households) {
                timer.start();
                const double improvement = household->plan(divided_d);
                plan_us += timer.stop_us();

                candidates.push_back({improvement, household.get()});
            }
            h_plan_compute.add(plan_us, household_count);

            std::vector<Household*> accepted_households;
            double best_improvement = 0.0;
            double total_accepted_improvement = 0.0;
            bool should_stop = false;

            timer.start();
            std::sort(
                candidates.begin(),
                candidates.end(),
                [](const auto& a, const auto& b) {
                    return a.first > b.first;
                }
            );

            best_improvement = candidates.empty() ? 0.0 : candidates.front().first;

            const int max_accepts = std::min(
                mu,
                static_cast<int>(candidates.size())
            );

            accepted_households.reserve(static_cast<std::size_t>(max_accepts));

            for (int j = 0; j < max_accepts; ++j) {
                const double improvement = candidates[static_cast<std::size_t>(j)].first;
                Household* household = candidates[static_cast<std::size_t>(j)].second;

                if (household == nullptr || improvement <= 0.0) {
                    continue;
                }

                accepted_households.push_back(household);
                total_accepted_improvement += improvement;
            }

            const int accepted_count = static_cast<int>(accepted_households.size());

            const double psm_e_min =
                static_cast<double>(household_count)
                * e_min
                / std::sqrt(static_cast<double>(mu));

            should_stop = best_improvement < psm_e_min || accepted_count == 0;
            c_iter_us += timer.stop_us();

            std::vector<Profile> accepted_diffs;
            accepted_diffs.reserve(static_cast<std::size_t>(accepted_count));

            long long accept_us = 0;
            for (Household* household : accepted_households) {
                timer.start();
                accepted_diffs.push_back(household->accept());
                accept_us += timer.stop_us();
            }
            h_accept_compute.add(accept_us, accepted_count);
            total_accepted_deltas += accepted_count;

            if (accepted_count > 0) {
                Profile aggregate_delta(static_cast<std::size_t>(intervals), 0.0);

#ifdef USE_SEAL
                std::vector<seal::Ciphertext> encrypted_diffs;
                encrypted_diffs.reserve(static_cast<std::size_t>(accepted_count));

                long long encrypt_delta_us = 0;
                for (const Profile& diff : accepted_diffs) {
                    timer.start();
                    seal::Ciphertext encrypted_diff = he.encrypt_profile(diff);
                    encrypt_delta_us += timer.stop_us();
                    h_encrypt_delta.cipher_bytes = static_cast<std::size_t>(encrypted_diff.save_size());
                    encrypted_diffs.push_back(std::move(encrypted_diff));
                }
                h_encrypt_delta.add(encrypt_delta_us, accepted_count);

                timer.start();
                seal::Ciphertext encrypted_delta_total = encrypted_diffs.front();
                b_init_delta_from_first.add(timer.stop_us(), 1);
                b_init_delta_from_first.cipher_bytes = static_cast<std::size_t>(encrypted_delta_total.save_size());

                long long add_delta_us = 0;
                for (std::size_t i = 1; i < encrypted_diffs.size(); ++i) {
                    timer.start();
                    he.add_inplace(encrypted_delta_total, encrypted_diffs[i]);
                    add_delta_us += timer.stop_us();
                }
                b_add_delta_remaining.add(add_delta_us, std::max(0, accepted_count - 1));
                b_add_delta_remaining.cipher_bytes = static_cast<std::size_t>(encrypted_delta_total.save_size());

                timer.start();
                aggregate_delta = he.decrypt_profile(encrypted_delta_total);
                c_decrypt_delta.add(timer.stop_us(), 1);
#else
                timer.start();
                aggregate_delta = accepted_diffs.front();
                b_init_delta_from_first.add(timer.stop_us(), 1);

                timer.start();
                for (std::size_t i = 1; i < accepted_diffs.size(); ++i) {
                    aggregate_delta = add_profiles(aggregate_delta, accepted_diffs[i]);
                }
                b_add_delta_remaining.add(timer.stop_us(), std::max(0, accepted_count - 1));
#endif

                timer.start();
                x = add_profiles(x, aggregate_delta);
                c_iter_us += timer.stop_us();
            }

            timer.start();
            if (!should_stop) {
                mu = std::min(
                    household_count,
                    std::max(
                        1,
                        static_cast<int>(std::floor(static_cast<double>(mu) / beta))
                    )
                );
            }
            c_iter_us += timer.stop_us();

            c_control_logic.add(c_iter_us, 1);

            if (!quiet) {
                std::cerr << "Simulation " << simulation
                          << " iteration " << iter
                          << " accepted " << accepted_count
                          << " / mu " << mu
                          << " best " << best_improvement
                          << " total accepted improvement " << total_accepted_improvement
                          << "\n";
            }

            if (should_stop) {
                break;
            }
        }

        if (!x.empty()) {
            checksum += x[0];
        }
    }

    Metric h_total {"H_total_household_side"};
    h_total.total_us = h_initial_device_planning.total_us
        + h_encrypt_x.total_us
        + h_plan_compute.total_us
        + h_accept_compute.total_us
        + h_encrypt_delta.total_us;
    h_total.count = simulation_count;
    h_total.cipher_bytes = std::max(h_encrypt_x.cipher_bytes, h_encrypt_delta.cipher_bytes);

    Metric b_total {"B_total_aggregation"};
    b_total.total_us = b_init_x_from_first.total_us
        + b_add_x_remaining.total_us
        + b_init_delta_from_first.total_us
        + b_add_delta_remaining.total_us;
    b_total.count = simulation_count;
    b_total.cipher_bytes = std::max(b_add_x_remaining.cipher_bytes, b_add_delta_remaining.cipher_bytes);

    Metric c_total {"C_total_decrypt_plus_control"};
    c_total.total_us = c_decrypt_x.total_us
        + c_decrypt_delta.total_us
        + c_control_logic.total_us;
    c_total.count = simulation_count;
    c_total.cipher_bytes = 0;

    Metric sim_total {"SIM_total_measured_H_B_C"};
    sim_total.total_us = h_total.total_us + b_total.total_us + c_total.total_us;
    sim_total.count = simulation_count;
    sim_total.cipher_bytes = std::max(h_total.cipher_bytes, b_total.cipher_bytes);

    std::vector<Metric> metrics = {
        h_initial_device_planning,
        h_encrypt_x,
        h_plan_compute,
        h_accept_compute,
        h_encrypt_delta,
        h_total,
        b_init_x_from_first,
        b_add_x_remaining,
        b_init_delta_from_first,
        b_add_delta_remaining,
        b_total,
        c_decrypt_x,
        c_decrypt_delta,
        c_control_logic,
        c_total,
        sim_total
    };

    std::cerr << "\n========================================\n";
    std::cerr << "H/B/C Full-Convergence Benchmark Report\n";
    std::cerr << "========================================\n";

    for (const Metric& metric : metrics) {
        print_metric_human(metric);
    }

    std::cerr << "[SUMMARY] total iterations: " << total_iterations << "\n";
    std::cerr << "[SUMMARY] avg iterations per simulation: "
              << static_cast<double>(total_iterations) / static_cast<double>(simulation_count)
              << "\n";
    std::cerr << "[SUMMARY] accepted delta ciphertexts/plain updates: " << total_accepted_deltas << "\n";
    std::cerr << "[SUMMARY] avg accepted deltas per simulation: "
              << static_cast<double>(total_accepted_deltas) / static_cast<double>(simulation_count)
              << "\n";
    std::cerr << "[SUMMARY] checksum: " << checksum << "\n";
    std::cerr << "[SUMMARY] size sink: " << g_size_sink << " double sink: " << g_double_sink << "\n";

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "scheme,metric,households,simulations,cache_mode,total_us,avg_us,count,cipher_bytes\n";

    for (const Metric& metric : metrics) {
        print_metric_csv(
            scheme,
            metric,
            household_count,
            simulation_count,
            cache_mode
        );
    }

    std::cout << "SUMMARY,total_iterations,"
              << household_count << ","
              << simulation_count << ","
              << cache_mode << ","
              << total_iterations << ","
              << static_cast<double>(total_iterations) / static_cast<double>(simulation_count) << ","
              << simulation_count << ",0\n";

    std::cout << "SUMMARY,accepted_delta_count,"
              << household_count << ","
              << simulation_count << ","
              << cache_mode << ","
              << total_accepted_deltas << ","
              << static_cast<double>(total_accepted_deltas) / static_cast<double>(simulation_count) << ","
              << simulation_count << ",0\n";

    return 0;
}