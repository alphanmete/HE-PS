#pragma once
#include "Device.hpp"
#include "OptAlg.hpp"
#include <random>

class Load : public Device {
public:
    explicit Load(std::mt19937& rng, double max_power = 5000.0);
    Profile init(const Profile& desired) override;
    double plan(const Profile& d) override;
    Profile accept() override;
    std::string name() const override { return "Load"; }
private:
    std::mt19937& rng_;
    double max_;
    Profile profile_, candidate_;
};

class Battery : public Device {
public:
    Profile init(const Profile& desired) override;
    double plan(const Profile& d) override;
    Profile accept() override;
    std::string name() const override { return "Battery"; }
private:
    Profile profile_, candidate_;
    double capacity_ = 3500.0;
    double max_power_ = 3700.0;
    double min_power_ = -3700.0;
    double initial_soc_ = 0.5 * capacity_;
    double tau_ = 4.0;
    OptAlg opt_;
};

class ElectricVehicle : public Device {
public:
    ElectricVehicle(std::mt19937& rng, int intervals);
    Profile init(const Profile& desired) override;
    double plan(const Profile& d) override;
    Profile accept() override;
    std::string name() const override { return "ElectricVehicle"; }
private:
    Profile profile_, candidate_;
    double capacity_ = 10000.0;
    Profile powers_ = {0, 4140, 4830, 5520, 6210, 6900, 7590, 8280, 8970, 9660, 10350, 11040};
    bool discrete_ = false;
    int start_time_ = 0;
    int end_time_ = 0;
    double tau_ = 4.0;
    double charge_request_ = 0.0;
    double initial_soc_ = 0.0;
    OptAlg opt_;
};

class HeatPump : public Device {
public:
    explicit HeatPump(std::mt19937& rng);
    Profile init(const Profile& desired) override;
    double plan(const Profile& d) override;
    Profile accept() override;
    std::string name() const override { return "HeatPump"; }
private:
    std::mt19937& rng_;
    Profile profile_, candidate_, heat_demand_;
    double capacity_ = 3500.0;
    double max_power_ = 3700.0;
    double min_power_ = 0.0;
    double initial_soc_ = 0.5 * capacity_;
    double tau_ = 4.0;
    OptAlg opt_;
};
