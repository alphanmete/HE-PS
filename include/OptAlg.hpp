#pragma once
#include "VectorUtils.hpp"

class OptAlg {
public:
    double fill_level = 0.0;

    Profile continuousBufferPlanning(
        const Profile& desired,
        double chargeRequired,
        double powerMin,
        double powerMax,
        Profile powerLimitsLower = {},
        Profile powerLimitsUpper = {},
        Profile prices = {},
        double beta = 1.0);

    Profile continuousBufferPlanningPositive(
        const Profile& desired,
        double chargeRequired,
        double powerMax,
        Profile powerLimitsUpper = {},
        Profile prices = {},
        double beta = 1.0);

    Profile continuousBufferPlanningPrices(
        double chargeRequired,
        double powerMax,
        Profile powerLimitsUpper,
        const Profile& prices);

    Profile discreteBufferPlanningPositive(
        const Profile& desired,
        double chargeRequired,
        Profile chargingPowers,
        Profile powerLimitsUpper = {},
        Profile prices = {},
        double beta = 1.0,
        Profile efficiency = {},
        Profile intervalMerge = {});

    Profile bufferPlanning(
        Profile desired,
        double targetSoC,
        double initialSoC,
        double capacity,
        Profile demand,
        Profile chargingPowers,
        double powerMin = 0.0,
        double powerMax = 0.0,
        Profile powerLimitsLower = {},
        Profile powerLimitsUpper = {},
        bool reactivePower = false,
        Profile prices = {},
        double beta = 1.0,
        Profile efficiency = {},
        Profile intervalMerge = {});
};
