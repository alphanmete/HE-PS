#include "SecureAggregator.hpp"

#include <algorithm>
#include <cstddef>

SecureAggregator::SecureAggregator(std::size_t profile_size)
    : SecureAggregator(profile_size, nullptr) {}

SecureAggregator::SecureAggregator(
    std::size_t profile_size,
    SimulationBenchmark* benchmark
)
    : profile_size_(profile_size),
      benchmark_(benchmark)
#ifdef USE_SEAL
      , he_(profile_size)
#endif
{
    reset();
}

void SecureAggregator::reset() {
#ifdef USE_SEAL
    if (benchmark_ != nullptr) {
        benchmark_->he_internal.start();
    }

    encrypted_total_ = he_.encrypt_profile(Profile(profile_size_, 0.0));

    if (benchmark_ != nullptr) {
        benchmark_->he_internal.stop();
        benchmark_->record_aggregate_cipher_bytes(
            static_cast<std::size_t>(encrypted_total_.save_size())
        );
    }
#else
    plaintext_total_ = Profile(profile_size_, 0.0);
#endif
}

void SecureAggregator::encrypt_and_add(const Profile& profile) {
    encrypt_and_add_impl(
        profile,
        true
    );
}

void SecureAggregator::encrypt_and_add_internal(const Profile& profile) {
    encrypt_and_add_impl(
        profile,
        false
    );
}

void SecureAggregator::encrypt_and_add_impl(
    const Profile& profile,
    bool count_as_household_submission
) {
#ifdef USE_SEAL
    seal::Ciphertext encrypted_profile;

    if (benchmark_ != nullptr && count_as_household_submission) {
        benchmark_->he_encrypt.start();
    } else if (benchmark_ != nullptr) {
        benchmark_->he_internal.start();
    }

    encrypted_profile = he_.encrypt_profile(profile);

    if (benchmark_ != nullptr && count_as_household_submission) {
        benchmark_->he_encrypt.stop();

        benchmark_->record_submitted_cipher_bytes(
            static_cast<std::size_t>(encrypted_profile.save_size())
        );
    } else if (benchmark_ != nullptr) {
        benchmark_->he_internal.stop();
    }

    if (benchmark_ != nullptr) {
        benchmark_->he_add.start();
    }

    he_.add_inplace(
        encrypted_total_,
        encrypted_profile
    );

    if (benchmark_ != nullptr) {
        benchmark_->he_add.stop();

        benchmark_->record_aggregate_cipher_bytes(
            static_cast<std::size_t>(encrypted_total_.save_size())
        );
    }
#else
    (void)count_as_household_submission;
    plaintext_total_ = add_profiles(
        plaintext_total_,
        profile
    );
#endif
}

Profile SecureAggregator::decrypt_aggregate() {
#ifdef USE_SEAL
    if (benchmark_ != nullptr) {
        benchmark_->he_decrypt.start();
    }

    Profile result = he_.decrypt_profile(encrypted_total_);

    if (benchmark_ != nullptr) {
        benchmark_->he_decrypt.stop();
    }

    return result;
#else
    return plaintext_total_;
#endif
}