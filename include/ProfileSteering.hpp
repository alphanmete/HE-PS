#pragma once

#include "Household.hpp"
#include "SecureAggregator.hpp"
#include "VectorUtils.hpp"

#include <memory>
#include <vector>

class ProfileSteering {
public:
    explicit ProfileSteering(std::vector<std::unique_ptr<Household>> households);

    Profile init(const Profile& desired);
    Profile iterative(double e_min, int max_iters);

private:
    std::vector<std::unique_ptr<Household>> households_;

    Profile p_;
    Profile x_;
};