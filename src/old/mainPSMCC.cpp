#include "ProfileSteeringPSM.hpp"
#include "Household.hpp"
#include "Devices.hpp"
#include "VectorUtils.hpp"
#include "BenchmarkTimer.hpp"

#include <cstdint>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <vector>

static void flush_cpu_cache() {
    constexpr size_t SIZE = 512 * 1024 * 1024; // 512 MB

    static std::vector<char> buffer(SIZE);

    volatile uint64_t sum = 0;

    for (size_t i = 0; i < SIZE; i += 64) {
        sum += buffer[i];
    }

    (void)sum;
}

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
    constexpr double e_min = 0.001; /// minimum improvement threshold
    constexpr int max_iters = 100;

    int household_count = 10;
    int simulation_count = 1;
    bool quiet = false;
    bool collect_metrics = false;

    if (argc > 1) {
        household_count = std::stoi(argv[1]);
    }

    if (argc > 2) {
        simulation_count = std::stoi(argv[2]);
    }

    for (int i = 3; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "quiet") {
            quiet = true;
        } else if (arg == "metrics") {
            collect_metrics = true;
        }
    }

    if (collect_metrics) {
        std::cerr << "[INFO] Metrics enabled.\n";
        std::cerr << "[INFO] CPU cache eviction is performed before every simulation.\n";
        std::cerr << "[INFO] The application code itself is sequential; no std::thread/OpenMP is used.\n";
        std::cerr << "[INFO] perf stat 'CPUs utilized' near 1.0 indicates single-core execution.\n\n";
    }

    SimulationBenchmark benchmark;
    SimulationBenchmark* benchmark_ptr =
        collect_metrics ? &benchmark : nullptr;

    double checksum = 0.0;

    if (quiet) {
        std::cout.setstate(std::ios_base::failbit);
    }

    for (int simulation = 0; simulation < simulation_count; ++simulation) {
        flush_cpu_cache();

        std::mt19937 rng(42 + simulation);

        Profile global_desired_profile(
            intervals,
            0.0
        );

        std::vector<std::unique_ptr<Household>> households;

        households.reserve(
            static_cast<size_t>(household_count)
        );

        for (int i = 0; i < household_count; ++i) {
            households.push_back(
                create_household(
                    i,
                    rng,
                    intervals
                )
            );
        }

        ProfileSteeringPSM ps(
            std::move(households),
            benchmark_ptr
        );

        Profile initial =
            ps.init(global_desired_profile);

        if (!quiet) {
            std::cout << "Initial aggregate prefix: ";
            print_profile_prefix(initial);
        }

        Profile result =
            ps.iterative(
                e_min,
                max_iters
            );

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

    if (collect_metrics) {
        benchmark.report(
            simulation_count,
            household_count
        );
    }

    std::cerr << "Households: " << household_count << "\n";
    std::cerr << "Simulations: " << simulation_count << "\n";
    std::cerr << "Mode: PSM-CC\n";
    std::cerr << "Metrics: " << (collect_metrics ? "enabled" : "disabled") << "\n";
    std::cerr << "Checksum: " << checksum << "\n";

    return 0;
}