#include "ProfileSteering.hpp"

#include <iostream>

ProfileSteering::ProfileSteering(std::vector<std::unique_ptr<Household>> households)
    : households_(std::move(households)) {}

Profile ProfileSteering::init(const Profile& global_desired) {
    p_ = global_desired;

    Profile local_zero_target(global_desired.size(), 0.0);

    SecureAggregator secure_aggregator(global_desired.size());

    for (auto& household : households_) {
        Profile household_profile = household->init(local_zero_target); /// inital desired profile is communicated with the households

        // Household sends plaintext profile to trusted HE layer.
        // HE layer encrypts and homomorphically aggregates.
        secure_aggregator.encrypt_and_add(household_profile);
    }

    // Profile Steering receives only the decrypted aggregate.
    x_ = secure_aggregator.decrypt_aggregate();

#ifdef USE_SEAL
#ifdef USE_CKKS
    std::cout << "[CKKS] Initial aggregate decrypted by ProfileSteering\n";
#else
    std::cout << "[BFV] Initial aggregate decrypted by ProfileSteering\n";
#endif
#else
    std::cout << "[Plain] Initial aggregate computed without encryption\n";
#endif

    return x_;
}

Profile ProfileSteering::iterative(double e_min, int max_iters) {
    SecureAggregator secure_aggregator(x_.size());

    // Start encrypted/plain aggregate from current x.
    secure_aggregator.encrypt_and_add(x_);

    for (int i = 0; i < max_iters; ++i) {
        double best_improvement = 0.0;
        Household* best_household = nullptr;

        Profile d = sub_profiles(x_, p_); /// x = aggregate community profile & p = desired profile

        for (auto& household : households_) {
            double improvement = household->plan(d); /// d = delta

            if (improvement > best_improvement) {
                best_improvement = improvement;
                best_household = household.get();
            }
        }

        if (best_household != nullptr) {
            Profile diff = best_household->accept();

            // Household sends plaintext diff to trusted HE layer.
            // HE layer encrypts and homomorphically adds it.
            secure_aggregator.encrypt_and_add(diff);

            // Profile Steering decrypts only updated aggregate.
            x_ = secure_aggregator.decrypt_aggregate();
        }

        std::cout << "Iteration " << i
                  << " -- Winner "
                  << (best_household ? best_household->name() : "None")
                  << " Improvement " << best_improvement << "\n";

        if (best_improvement < e_min) {
            break;
        }
    }

    return x_;
}