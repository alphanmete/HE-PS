#include "Household.hpp"

#include <utility>

Household::Household(std::string name, std::vector<std::unique_ptr<Device>> devices)
    : name_(std::move(name)), devices_(std::move(devices)) {}

Profile Household::init(const Profile& desired) {
    profile_ = Profile(desired.size(), 0.0);

    for (auto& device : devices_) {
        Profile r = device->init(desired); /// individual household profile r => x1
        profile_ = add_profiles(profile_, r);
    }

    return profile_;
}

double Household::plan(const Profile& d) {
    double best_improvement = 0.0;
    best_device_ = nullptr;

    for (auto& device : devices_) {
        double improvement = device->plan(d);

        if (improvement > best_improvement) {
            best_improvement = improvement;
            best_device_ = device.get();
        }
    }

    return best_improvement;
}

Profile Household::accept() {
    if (best_device_ == nullptr) {
        return Profile(profile_.size(), 0.0);
    }

    Profile diff = best_device_->accept();
    profile_ = add_profiles(profile_, diff);
    best_device_ = nullptr;

    return diff;
}

const Profile& Household::profile() const {
    return profile_;
}

const std::string& Household::name() const {
    return name_;
}