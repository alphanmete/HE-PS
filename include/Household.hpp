#pragma once

#include "Device.hpp"
#include "VectorUtils.hpp"

#include <memory>
#include <string>
#include <vector>

class Household {
public:
    Household(std::string name, std::vector<std::unique_ptr<Device>> devices);

    Profile init(const Profile& desired);
    double plan(const Profile& d);
    Profile accept();

    const Profile& profile() const;
    const std::string& name() const;

private:
    std::string name_;
    std::vector<std::unique_ptr<Device>> devices_;
    Profile profile_;

    Device* best_device_ = nullptr;
};