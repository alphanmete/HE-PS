#pragma once

#include <chrono>
#include <cstddef>
#include <iostream>
#include <string>
#include <utility>

class AccumulatingTimer {
public:
    explicit AccumulatingTimer(std::string label)
        : label_(std::move(label)) {}

    void start() {
        start_ = Clock::now();
        running_ = true;
    }

    long long stop() {
        if (!running_) {
            return 0;
        }

        const auto end = Clock::now();

        const long long us =
            std::chrono::duration_cast<std::chrono::microseconds>(
                end - start_
            ).count();

        total_us_ += us;
        ++count_;
        running_ = false;

        return us;
    }

    void report(std::ostream& out = std::cout) const {
        out << "[TIMER] " << label_
            << " | total: " << total_us_ << " us"
            << " | calls: " << count_;

        if (count_ > 0) {
            out << " | avg: " << static_cast<double>(total_us_) / static_cast<double>(count_) << " us";
        }

        out << "\n";
    }

    long long total_us() const {
        return total_us_;
    }

    long long count() const {
        return count_;
    }

private:
    using Clock = std::chrono::steady_clock;

    std::string label_;
    long long total_us_ = 0;
    long long count_ = 0;
    bool running_ = false;
    Clock::time_point start_;
};

struct SimulationBenchmark {
    // Household / Raspberry-Pi relevant.
    AccumulatingTimer hh_init_compute {
        "Household init compute        [household/Pi]"
    };

    AccumulatingTimer hh_plan_compute {
        "Household plan compute        [household/Pi]"
    };

    AccumulatingTimer hh_accept_compute {
        "Household accept compute      [household/Pi]"
    };

    AccumulatingTimer he_encrypt {
        "HE encrypt submitted profile  [household/Pi]"
    };

    // Server / coordinator side.
    AccumulatingTimer he_add {
        "HE homomorphic add            [server]"
    };

    AccumulatingTimer he_decrypt {
        "HE decrypt aggregate          [coordinator]"
    };

    AccumulatingTimer coord_compute {
        "Coordinator steering compute  [coordinator]"
    };

    // Internal HE work that is not a household submission.
    // Example: encrypting zero or encrypting current aggregate x_ for bookkeeping.
    AccumulatingTimer he_internal {
        "HE internal setup/bookkeeping [server/coordinator]"
    };

    // Submitted ciphertexts: household profile/update ciphertexts.
    std::size_t submitted_cipher_max_bytes = 0;
    std::size_t submitted_cipher_total_bytes = 0;
    long long submitted_cipher_count = 0;

    // Aggregate ciphertexts after additions.
    std::size_t aggregate_cipher_max_bytes = 0;
    std::size_t aggregate_cipher_total_bytes = 0;
    long long aggregate_cipher_count = 0;

    void record_submitted_cipher_bytes(std::size_t bytes) {
        submitted_cipher_max_bytes = std::max(submitted_cipher_max_bytes, bytes);
        submitted_cipher_total_bytes += bytes;
        ++submitted_cipher_count;
    }

    void record_aggregate_cipher_bytes(std::size_t bytes) {
        aggregate_cipher_max_bytes = std::max(aggregate_cipher_max_bytes, bytes);
        aggregate_cipher_total_bytes += bytes;
        ++aggregate_cipher_count;
    }

    long long household_pi_total_us() const {
        return hh_init_compute.total_us()
             + hh_plan_compute.total_us()
             + hh_accept_compute.total_us()
             + he_encrypt.total_us();
    }

    long long server_total_us() const {
        return he_add.total_us()
             + he_decrypt.total_us()
             + coord_compute.total_us()
             + he_internal.total_us();
    }

    void report(
        int simulations,
        int households,
        std::ostream& out = std::cout
    ) const {
        out << "\n========================================\n"
            << "  Accumulated Benchmark Report\n"
            << "  Simulations : " << simulations << "\n"
            << "  Households  : " << households << "\n"
            << "========================================\n";

        out << "\n-- Household-side / Raspberry-Pi relevant --\n";
        hh_init_compute.report(out);
        hh_plan_compute.report(out);
        hh_accept_compute.report(out);
        he_encrypt.report(out);

        out << "[TOTAL] Household/Pi relevant"
            << " | total: " << household_pi_total_us() << " us";

        if (simulations > 0) {
            out << " | avg per simulation: "
                << static_cast<double>(household_pi_total_us()) / static_cast<double>(simulations)
                << " us";
        }

        out << "\n";

        out << "\n-- Server / Coordinator --\n";
        he_add.report(out);
        he_decrypt.report(out);
        coord_compute.report(out);
        he_internal.report(out);

        out << "[TOTAL] Server/coordinator"
            << " | total: " << server_total_us() << " us";

        if (simulations > 0) {
            out << " | avg per simulation: "
                << static_cast<double>(server_total_us()) / static_cast<double>(simulations)
                << " us";
        }

        out << "\n";

        if (submitted_cipher_count > 0) {
            out << "\n-- Submitted Ciphertext Size --\n"
                << "[CIPHER] max submitted: " << submitted_cipher_max_bytes << " B"
                << " | avg submitted: "
                << static_cast<double>(submitted_cipher_total_bytes) / static_cast<double>(submitted_cipher_count)
                << " B"
                << " | count: " << submitted_cipher_count
                << "\n";
        }

        if (aggregate_cipher_count > 0) {
            out << "\n-- Aggregate Ciphertext Size --\n"
                << "[CIPHER] max aggregate: " << aggregate_cipher_max_bytes << " B"
                << " | avg aggregate: "
                << static_cast<double>(aggregate_cipher_total_bytes) / static_cast<double>(aggregate_cipher_count)
                << " B"
                << " | count: " << aggregate_cipher_count
                << "\n";
        }

        out << "========================================\n\n";
    }
};