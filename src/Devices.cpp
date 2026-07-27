#include "Devices.hpp"
#include <algorithm>
#include <cassert>
#include <numeric>

Load::Load(std::mt19937& rng, double max_power) : rng_(rng), max_(max_power) {}

Profile Load::init(const Profile& desired) {
    std::uniform_real_distribution<double> dist(0.0, max_);
    profile_.clear(); profile_.reserve(desired.size());
    for (size_t i = 0; i < desired.size(); ++i) profile_.push_back(dist(rng_));
    return profile_;
}

double Load::plan(const Profile& d) {
    require_same_size(d, profile_);
    Profile p_m = sub_profiles(profile_, d);
    candidate_ = profile_;
    return improvement_score(profile_, candidate_, p_m);
}

Profile Load::accept() {
    Profile diff = sub_profiles(candidate_, profile_);
    profile_ = candidate_;
    return diff;
}

Profile Battery::init(const Profile& desired) {
    profile_ = Profile(desired.size(), 0.0);
    return profile_;
}

double Battery::plan(const Profile& d) {
    Profile p_m = sub_profiles(profile_, d);
    candidate_ = opt_.bufferPlanning(
        p_m,
        initial_soc_ * tau_,
        initial_soc_ * tau_,
        capacity_ * tau_,
        Profile(p_m.size(), 0.0),
        {},
        min_power_,
        max_power_,
        {}, {}, false, {}, 1.0);
    return improvement_score(profile_, candidate_, p_m);
}

Profile Battery::accept() {
    Profile diff = sub_profiles(candidate_, profile_);
    profile_ = candidate_;
    return diff;
}

ElectricVehicle::ElectricVehicle(std::mt19937& rng, int intervals) {
    std::uniform_int_distribution<int> start_dist(7 * 4, 12 * 4);
    std::uniform_int_distribution<int> end_dist(15 * 4, 22 * 4);
    std::uniform_int_distribution<int> charge_dist(1000, 10000);
    start_time_ = std::clamp(start_dist(rng), 0, intervals - 1);
    end_time_ = std::clamp(end_dist(rng), start_time_ + 1, intervals);
    charge_request_ = static_cast<double>(charge_dist(rng));
    initial_soc_ = capacity_ - charge_request_;
}

Profile ElectricVehicle::init(const Profile& desired) {
    profile_ = Profile(desired.size(), 0.0);
    plan(desired);
    accept();
    return profile_;
}

double ElectricVehicle::plan(const Profile& d) {
    Profile p_m = sub_profiles(profile_, d);
    Profile window = slice(p_m, static_cast<size_t>(start_time_), static_cast<size_t>(end_time_));
    Profile planned_window;
    if (!discrete_) {
        planned_window = opt_.bufferPlanning(
            window,
            capacity_ * tau_,
            initial_soc_ * tau_,
            capacity_ * tau_,
            Profile(window.size(), 0.0),
            {},
            powers_[0],
            powers_[1],
            {}, {}, false, {}, 1.0);
    } else {
        planned_window = opt_.discreteBufferPlanningPositive(
            window,
            charge_request_ * tau_,
            powers_,
            {}, {}, 1.0);
    }
    candidate_ = Profile(p_m.size(), 0.0);
    for (int i = start_time_; i < end_time_; ++i) candidate_[static_cast<size_t>(i)] = planned_window[static_cast<size_t>(i - start_time_)];
    return improvement_score(profile_, candidate_, p_m);
}

Profile ElectricVehicle::accept() {
    Profile diff = sub_profiles(candidate_, profile_);
    profile_ = candidate_;
    return diff;
}

HeatPump::HeatPump(std::mt19937& rng) : rng_(rng) {}

Profile HeatPump::init(const Profile& desired) {
    std::uniform_real_distribution<double> dist(0.0, max_power_ * 1.5);
    heat_demand_.clear(); heat_demand_.reserve(desired.size());
    for (size_t i = 0; i < desired.size(); ++i) heat_demand_.push_back(dist(rng_));
    profile_ = Profile(desired.size(), 0.0);
    plan(desired);
    accept();
    return profile_;
}

double HeatPump::plan(const Profile& d) {
    Profile p_m = sub_profiles(profile_, d);
    candidate_ = opt_.bufferPlanning(
        p_m,
        initial_soc_ * tau_,
        initial_soc_ * tau_,
        capacity_ * tau_,
        heat_demand_,
        {},
        min_power_,
        max_power_,
        {}, {}, false, {}, 1.0);
    return improvement_score(profile_, candidate_, p_m);
}

Profile HeatPump::accept() {
    Profile diff = sub_profiles(candidate_, profile_);
    profile_ = candidate_;
    return diff;
}
