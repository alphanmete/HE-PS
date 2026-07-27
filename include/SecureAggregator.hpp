#pragma once

#include "VectorUtils.hpp"
#include "BenchmarkTimer.hpp"

#ifdef USE_SEAL
#ifdef USE_CKKS
#include "HEAggregatorCKKS.hpp"
#else
#include "HEAggregator.hpp"
#endif
#endif

class SecureAggregator {
public:
    explicit SecureAggregator(std::size_t profile_size);

    SecureAggregator(
        std::size_t profile_size,
        SimulationBenchmark* benchmark
    );

    void reset();

    // Household-submitted profile/update.
    // This counts as Pi-relevant encryption when metrics are enabled.
    void encrypt_and_add(const Profile& profile);

    // Internal coordinator/server bookkeeping.
    // This is NOT counted as household/Pi encryption.
    void encrypt_and_add_internal(const Profile& profile);

    Profile decrypt_aggregate();

private:
    void encrypt_and_add_impl(
        const Profile& profile,
        bool count_as_household_submission
    );

    std::size_t profile_size_;
    SimulationBenchmark* benchmark_ = nullptr;

#ifdef USE_SEAL
#ifdef USE_CKKS
    HEAggregatorCKKS he_;
    seal::Ciphertext encrypted_total_;
#else
    HEAggregator he_;
    seal::Ciphertext encrypted_total_;
#endif
#else
    Profile plaintext_total_;
#endif
};