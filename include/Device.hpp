#pragma once
#include "VectorUtils.hpp"
#include <string>

class Device {
public:
    virtual ~Device() = default;
    virtual Profile init(const Profile& desired) = 0;
    virtual double plan(const Profile& d) = 0;
    virtual Profile accept() = 0;
    virtual std::string name() const = 0;
};
