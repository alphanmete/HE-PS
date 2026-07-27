#pragma once

#include "VectorUtils.hpp"
#include "BenchmarkTimer.hpp"
#include "SecureAggregator.hpp"
#include "Household.hpp"

#include <memory>
#include <vector>

class ProfileSteeringPSM {
public:
    explicit ProfileSteeringPSM(
        std::vector<std::unique_ptr<Household>> households
    );

    ProfileSteeringPSM(
        std::vector<std::unique_ptr<Household>> households,
        SimulationBenchmark& benchmark
    );

    ProfileSteeringPSM(
        std::vector<std::unique_ptr<Household>> households,
        SimulationBenchmark* benchmark
    );

    Profile init(const Profile& global_desired);

    Profile iterative(double e_min, int max_iters);

private:
    std::vector<std::unique_ptr<Household>> households_;
    SimulationBenchmark* benchmark_ = nullptr;
    Profile p_;
    Profile x_;
};