#include "ProfileSteeringPSM.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <utility>
#include <vector>

static Profile scale_profile(
    const Profile& profile,
    double factor
) {
    Profile scaled(profile.size(), 0.0);

    for (size_t i = 0; i < profile.size(); ++i) {
        scaled[i] = profile[i] * factor;
    }

    return scaled;
}

ProfileSteeringPSM::ProfileSteeringPSM(
    std::vector<std::unique_ptr<Household>> households
)
    : households_(std::move(households)),
      benchmark_(nullptr) {}

ProfileSteeringPSM::ProfileSteeringPSM(
    std::vector<std::unique_ptr<Household>> households,
    SimulationBenchmark& benchmark
)
    : households_(std::move(households)),
      benchmark_(&benchmark) {}

ProfileSteeringPSM::ProfileSteeringPSM(
    std::vector<std::unique_ptr<Household>> households,
    SimulationBenchmark* benchmark
)
    : households_(std::move(households)),
      benchmark_(benchmark) {}

Profile ProfileSteeringPSM::init(const Profile& global_desired) {
    p_ = global_desired;

    Profile local_zero_target(
        global_desired.size(),
        0.0
    );

    SecureAggregator secure_aggregator(
        global_desired.size(),
        benchmark_
    );

    for (auto& household : households_) {
        if (benchmark_ != nullptr) {
            benchmark_->hh_init_compute.start();
        }

        Profile household_profile =
            household->init(local_zero_target);

        if (benchmark_ != nullptr) {
            benchmark_->hh_init_compute.stop();
        }

        // Household-submitted initial profile.
        // Counts as household/Pi-side encryption when metrics are enabled.
        secure_aggregator.encrypt_and_add(household_profile);
    }

    x_ = secure_aggregator.decrypt_aggregate();

#ifdef USE_SEAL
#ifdef USE_CKKS
    std::cout << "[CKKS-PSM] Initial aggregate decrypted by ProfileSteeringPSM\n";
#else
    std::cout << "[BFV-PSM] Initial aggregate decrypted by ProfileSteeringPSM\n";
#endif
#else
    std::cout << "[Plain-PSM] Initial aggregate computed without encryption\n";
#endif

    return x_;
}

Profile ProfileSteeringPSM::iterative(
    double e_min,
    int max_iters
) {
    SecureAggregator secure_aggregator(
        x_.size(),
        benchmark_
    );

    // This seeds the aggregate with the current X.
    // It is internal coordinator/server bookkeeping, not a household submission.
    secure_aggregator.encrypt_and_add_internal(x_);

    int mu = static_cast<int>(households_.size());
    constexpr double beta = 2.0;

    for (int i = 0; i < max_iters; ++i) {
        if (mu < 1) {
            mu = 1;
        }

        if (benchmark_ != nullptr) {
            benchmark_->coord_compute.start();
        }

        Profile d = sub_profiles(
            x_,
            p_
        );

        Profile divided_d = scale_profile(
            d,
            1.0 / static_cast<double>(mu)
        );

        std::vector<std::pair<double, Household*>> candidates;
        candidates.reserve(households_.size());

        if (benchmark_ != nullptr) {
            benchmark_->coord_compute.stop();
        }

        for (auto& household : households_) {
            if (benchmark_ != nullptr) {
                benchmark_->hh_plan_compute.start();
            }

            double improvement =
                household->plan(divided_d);

            if (benchmark_ != nullptr) {
                benchmark_->hh_plan_compute.stop();
            }

            candidates.push_back({
                improvement,
                household.get()
            });
        }

        if (benchmark_ != nullptr) {
            benchmark_->coord_compute.start();
        }

        std::sort(
            candidates.begin(),
            candidates.end(),
            [](const auto& a, const auto& b) {
                return a.first > b.first;
            }
        );

        int accepted_count = 0;

        double best_improvement =
            candidates.empty() ? 0.0 : candidates.front().first;

        double total_accepted_improvement = 0.0;

        const int max_accepts = std::min(
            mu,
            static_cast<int>(candidates.size())
        );

        if (benchmark_ != nullptr) {
            benchmark_->coord_compute.stop();
        }

        for (int j = 0; j < max_accepts; ++j) {
            const double improvement =
                candidates[static_cast<size_t>(j)].first;

            Household* household =
                candidates[static_cast<size_t>(j)].second;

            if (household == nullptr || improvement <= 0.0) {
                continue;
            }

            if (benchmark_ != nullptr) {
                benchmark_->hh_accept_compute.start();
            }

            Profile diff = household->accept();

            if (benchmark_ != nullptr) {
                benchmark_->hh_accept_compute.stop();
            }

            // Household-submitted accepted update.
            // Counts as household/Pi-side encryption when metrics are enabled.
            secure_aggregator.encrypt_and_add(diff);

            ++accepted_count;
            total_accepted_improvement += improvement;
        }

        if (accepted_count > 0) {
            x_ = secure_aggregator.decrypt_aggregate();
        }

        std::cout << "Iteration " << i
                  << " -- Accepted " << accepted_count
                  << " / mu " << mu
                  << " Best improvement " << best_improvement
                  << " Total accepted improvement " << total_accepted_improvement
                  << "\n";

        if (benchmark_ != nullptr) {
            benchmark_->coord_compute.start();
        }

        const double psm_e_min =
            static_cast<double>(households_.size())
            * e_min
            / std::sqrt(static_cast<double>(mu));

        const bool should_stop =
            best_improvement < psm_e_min || accepted_count == 0;

        if (!should_stop) {
            mu = std::min(
                static_cast<int>(households_.size()),
                std::max(
                    1,
                    static_cast<int>(
                        std::floor(static_cast<double>(mu) / beta)
                    )
                )
            );
        }

        if (benchmark_ != nullptr) {
            benchmark_->coord_compute.stop();
        }

        if (should_stop) {
            break;
        }
    }

    return x_;
}