#include "ProfileSteering.hpp"
#include "Household.hpp"
#include "Devices.hpp"
#include "VectorUtils.hpp"

#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <vector>

static std::unique_ptr<Household> create_household(
    int id,
    std::mt19937& rng,
    int intervals
) {
    std::vector<std::unique_ptr<Device>> devices;

    devices.push_back(std::make_unique<Load>(rng));
    devices.push_back(std::make_unique<Battery>());
    devices.push_back(std::make_unique<ElectricVehicle>(rng, intervals));
    devices.push_back(std::make_unique<HeatPump>(rng));

    return std::make_unique<Household>(
        "Household_" + std::to_string(id),
        std::move(devices)
    );
}

int main(int argc, char* argv[]) {
    constexpr int intervals = 96;

    int household_count = 10;
    if (argc > 1) {
        household_count = std::stoi(argv[1]);
    }

    int simulation_count = 1;
    if (argc > 2) {
        simulation_count = std::stoi(argv[2]);
    }

    bool quiet = false;
    if (argc > 3) {
        quiet = std::string(argv[3]) == "quiet";
    }

    constexpr double e_min = 0.001; /// minimum improvement threshold
    constexpr int max_iters = 100;

    double checksum = 0.0;

    if (quiet) {
        std::cout.setstate(std::ios_base::failbit);
    }

    for (int simulation = 0; simulation < simulation_count; ++simulation) {
        std::mt19937 rng(42 + simulation);

        Profile global_desired_profile(intervals, 0.0);

        std::vector<std::unique_ptr<Household>> households;

        for (int i = 0; i < household_count; ++i) {
            households.push_back(create_household(i, rng, intervals));
        }

        ProfileSteering ps(std::move(households));

        Profile initial = ps.init(global_desired_profile);

        if (!quiet) {
            std::cout << "Initial aggregate prefix: ";
            print_profile_prefix(initial);
        }

        Profile result = ps.iterative(e_min, max_iters);

        if (!quiet) {
            std::cout << "Final aggregate prefix:   ";
            print_profile_prefix(result);
        }

        /// Checksum prevents compiler/runtime from treating the simulation result as unused.
        if (!result.empty()) {
            checksum += result[0];
        }
    }

    if (quiet) {
        std::cout.clear();
    }

    std::cerr << "Households: " << household_count << "\n";
    std::cerr << "Simulations: " << simulation_count << "\n";
    std::cerr << "Checksum: " << checksum << "\n";

    return 0;
}