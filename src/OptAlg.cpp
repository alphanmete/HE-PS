#include "OptAlg.hpp"
#include <algorithm>
#include <cassert>
#include <limits>
#include <numeric>
#include <stdexcept>

static Profile zeros(size_t n) { return Profile(n, 0.0); }
static Profile filled(size_t n, double v) { return Profile(n, v); }
static bool has_positive_lower(const Profile& v) { return std::any_of(v.begin(), v.end(), [](double x){ return x > 0.0; }); }

Profile OptAlg::continuousBufferPlanning(
    const Profile& desired,
    double chargeRequired,
    double powerMin,
    double powerMax,
    Profile powerLimitsLower,
    Profile powerLimitsUpper,
    Profile prices,
    double beta) {
    if (prices.empty()) prices = zeros(desired.size());
    bool positiveLowerBound = has_positive_lower(powerLimitsLower);

    if (powerMin < -0.0001 || powerMin > 0.0001 || positiveLowerBound) {
        if (powerLimitsLower.size() != desired.size() || powerLimitsUpper.size() != desired.size()) {
            Profile desiredNew(desired.size());
            for (size_t i = 0; i < desired.size(); ++i) desiredNew[i] = desired[i] - powerMin;
            Profile result = continuousBufferPlanningPositive(desiredNew, chargeRequired - powerMin * desired.size(), powerMax - powerMin, {}, prices, beta);
            for (double& v : result) v += powerMin;
            return result;
        }

        Profile result;
        Profile upperLimits = filled(desired.size(), powerMax);
        Profile lowerLimits = filled(desired.size(), powerMin);
        Profile remaining = zeros(desired.size());
        double totalLower = 0.0, totalUpper = 0.0;
        for (size_t i = 0; i < desired.size(); ++i) {
            if (powerLimitsLower[i] > powerLimitsUpper[i]) throw std::runtime_error("lower power limit exceeds upper power limit");
            lowerLimits[i] = std::max(powerMin, powerLimitsLower[i]); totalLower += lowerLimits[i];
            upperLimits[i] = std::min(powerMax, powerLimitsUpper[i]); totalUpper += upperLimits[i];
        }
        if (chargeRequired < powerMin * desired.size()) return filled(desired.size(), powerMin);
        if (chargeRequired > powerMax * desired.size()) return filled(desired.size(), powerMax);

        if (chargeRequired < totalLower) {
            auto sortedLower = lowerLimits; std::sort(sortedLower.begin(), sortedLower.end());
            size_t position = 0; double underLimits = totalLower - chargeRequired; double breakpoint = 0.0;
            while (position < sortedLower.size() && (sortedLower[position] - powerMin) < (underLimits / static_cast<double>(desired.size() - position))) {
                underLimits -= sortedLower[position] - powerMin; breakpoint = sortedLower[position]; ++position;
                if (underLimits < 0.0001) underLimits = 0.0;
            }
            result.reserve(desired.size());
            for (size_t i = 0; i < desired.size(); ++i)
                result.push_back(lowerLimits[i] > breakpoint ? lowerLimits[i] - (underLimits / static_cast<double>(desired.size() - position)) : powerMin);
            fill_level = breakpoint; return result;
        }

        if (chargeRequired > totalUpper) {
            for (size_t i = 0; i < desired.size(); ++i) remaining[i] = powerMax - upperLimits[i];
            auto sortedRemaining = remaining; std::sort(sortedRemaining.begin(), sortedRemaining.end());
            double overLimits = chargeRequired - totalUpper, breakpoint = 0.0; size_t position = 0;
            while (position < desired.size() && position < sortedRemaining.size() &&
                   (overLimits / static_cast<double>(desired.size() - position) > sortedRemaining[position])) {
                overLimits -= sortedRemaining[position]; breakpoint = sortedRemaining[position]; ++position;
            }
            result.reserve(desired.size());
            for (size_t i = 0; i < desired.size(); ++i)
                result.push_back(remaining[i] > breakpoint ? upperLimits[i] + overLimits / static_cast<double>(desired.size() - position) : powerMax);
            fill_level = breakpoint; return result;
        }

        Profile desiredNew(desired.size()), powerLimitsUpperNew(desired.size());
        for (size_t i = 0; i < desired.size(); ++i) {
            desiredNew[i] = desired[i] - lowerLimits[i];
            powerLimitsUpperNew[i] = upperLimits[i] - lowerLimits[i];
        }
        result = continuousBufferPlanningPositive(desiredNew, chargeRequired - totalLower, powerMax - powerMin, powerLimitsUpperNew, prices, beta);
        for (size_t i = 0; i < result.size(); ++i) result[i] += lowerLimits[i];
        return result;
    }

    return continuousBufferPlanningPositive(desired, chargeRequired, powerMax, powerLimitsUpper, prices, beta);
}

Profile OptAlg::continuousBufferPlanningPositive(
    const Profile& desired,
    double chargeRequired,
    double powerMax,
    Profile powerLimitsUpper,
    Profile prices,
    double beta) {
    if (prices.empty()) prices = zeros(desired.size());
    Profile result = zeros(desired.size());
    double remainingCharge = chargeRequired;
    if (chargeRequired <= 0.0) return result;

    Profile powerLimits = filled(desired.size(), powerMax);
    if (powerLimitsUpper.size() == desired.size()) {
        for (size_t i = 0; i < desired.size(); ++i) {
            powerLimits[i] = std::min(powerLimitsUpper[i], powerMax);
            if (powerLimits[i] < 0.0 && powerLimits[i] >= -0.0001) powerLimits[i] = 0.0;
            if (powerLimits[i] < -0.0001) throw std::runtime_error("negative power limit");
        }
    }
    if (chargeRequired > powerMax * desired.size()) return filled(desired.size(), powerMax);

    if (powerLimitsUpper.size() == desired.size()) {
        double totalAvailable = 0.0; Profile remaining = zeros(desired.size());
        for (size_t i = 0; i < desired.size(); ++i) { totalAvailable += powerLimits[i]; remaining[i] = powerMax - powerLimits[i]; }
        if (totalAvailable < chargeRequired) {
            auto sortedRemaining = remaining; std::sort(sortedRemaining.begin(), sortedRemaining.end());
            double overLimits = chargeRequired - totalAvailable, breakpoint = 0.0; size_t position = 0;
            while (position < desired.size() && position < sortedRemaining.size() &&
                   (overLimits / static_cast<double>(desired.size() - position) > sortedRemaining[position])) {
                overLimits -= sortedRemaining[position]; breakpoint = sortedRemaining[position]; ++position;
            }
            for (size_t i = 0; i < desired.size(); ++i)
                result[i] = remaining[i] > breakpoint ? powerLimits[i] + overLimits / static_cast<double>(desired.size() - position) : powerMax;
            fill_level = breakpoint; return result;
        }
    }

    if (beta == 0.0) return continuousBufferPlanningPrices(chargeRequired, powerMax, powerLimitsUpper, prices);

    int lower = 0, upper = -1;
    Profile lowerLevels = result, upperLevels = result;
    if (beta == 1.0) prices = zeros(desired.size());
    if (prices.size() != desired.size()) throw std::runtime_error("prices length mismatch");
    for (size_t i = 0; i < desired.size(); ++i) {
        double lvl = (prices[i] / (2.0 * beta)) - desired[i];
        lowerLevels[i] = lvl;
        upperLevels[i] = lvl + powerLimits[i];
    }
    auto sortedLower = lowerLevels, sortedUpper = upperLevels;
    std::sort(sortedLower.begin(), sortedLower.end()); std::sort(sortedUpper.begin(), sortedUpper.end());
    double breakpoint = sortedLower[0];

    while (remainingCharge > 0.0 && upper + 1 < static_cast<int>(desired.size())) {
        double denom = static_cast<double>(lower - upper);
        if (lower + 1 == static_cast<int>(desired.size())) {
            double change = std::min(remainingCharge / denom, sortedUpper[upper + 1] - breakpoint);
            breakpoint += change; remainingCharge -= change * denom; ++upper;
        } else if (upper == lower) {
            breakpoint += sortedLower[lower + 1] - breakpoint; ++lower;
        } else if (sortedLower[lower + 1] < sortedUpper[upper + 1]) {
            double change = std::min(remainingCharge / denom, sortedLower[lower + 1] - breakpoint);
            breakpoint += change; remainingCharge -= change * denom; ++lower;
        } else {
            double change = std::min(remainingCharge / denom, sortedUpper[upper + 1] - breakpoint);
            breakpoint += change; remainingCharge -= change * denom; ++upper;
        }
    }
    for (size_t i = 0; i < desired.size(); ++i) {
        if (breakpoint >= upperLevels[i]) result[i] = powerLimits[i];
        else if (breakpoint > lowerLevels[i]) result[i] = breakpoint - lowerLevels[i];
    }
    fill_level = breakpoint; return result;
}

Profile OptAlg::continuousBufferPlanningPrices(double chargeRequired, double powerMax, Profile powerLimitsUpper, const Profile& prices) {
    Profile result = zeros(prices.size());
    Profile powerLimits = filled(prices.size(), powerMax);
    if (powerLimitsUpper.size() == prices.size()) for (size_t i = 0; i < prices.size(); ++i) powerLimits[i] = std::min(powerLimitsUpper[i], powerMax);
    std::vector<std::pair<double, size_t>> sorted; sorted.reserve(prices.size());
    for (size_t i = 0; i < prices.size(); ++i) sorted.push_back({prices[i], i});
    std::sort(sorted.begin(), sorted.end());
    double remainingCharge = chargeRequired; size_t i = 0;
    while (remainingCharge > 0.0 && i < prices.size()) {
        size_t idx = sorted[i].second;
        if (remainingCharge > powerLimits[idx]) { result[idx] = powerLimits[idx]; remainingCharge -= powerLimits[idx]; ++i; }
        else { result[idx] = remainingCharge; remainingCharge = 0.0; }
    }
    return result;
}

Profile OptAlg::discreteBufferPlanningPositive(const Profile& desired, double chargeRequired, Profile chargingPowers, Profile powerLimitsUpper, Profile prices, double beta, Profile efficiency, Profile intervalMerge) {
    if (efficiency.empty()) efficiency = filled(chargingPowers.size(), 1.0);
    if (prices.empty()) prices = zeros(desired.size());
    if (intervalMerge.empty()) intervalMerge = filled(desired.size(), 1.0);
    std::sort(chargingPowers.begin(), chargingPowers.end());
    Profile result = zeros(desired.size());
    double remainingCharge = chargeRequired;
    if (chargingPowers.size() < 2) throw std::runtime_error("discrete planning requires at least two charging powers");
    std::vector<std::pair<double, std::pair<size_t, size_t>>> slopes;
    for (size_t i = 0; i < desired.size(); ++i) {
        if (powerLimitsUpper.empty() || chargingPowers[1] <= powerLimitsUpper[i]) {
            double slope = ((prices[i] * chargingPowers[1] * efficiency[1] + beta * intervalMerge[i] * std::pow(chargingPowers[1] * efficiency[1] - desired[i], 2))
                          - (prices[i] * chargingPowers[0] * efficiency[0] + beta * intervalMerge[i] * std::pow(chargingPowers[0] * efficiency[0] - desired[i], 2)))
                         / (intervalMerge[i] * ((chargingPowers[1] * efficiency[1]) - (chargingPowers[0] * efficiency[0])));
            slopes.push_back({slope, {i, 1}});
        }
    }
    while (remainingCharge > 0.001 && !slopes.empty()) {
        std::sort(slopes.begin(), slopes.end());
        size_t i = slopes.front().second.first, j = slopes.front().second.second;
        double sigma = std::min(remainingCharge, intervalMerge[i] * (chargingPowers[j] - chargingPowers[j - 1]));
        result[i] += sigma / intervalMerge[i]; remainingCharge -= sigma; slopes.erase(slopes.begin());
        if (j < chargingPowers.size() - 1 && (powerLimitsUpper.empty() || chargingPowers[j + 1] <= powerLimitsUpper[i])) {
            double slope = ((prices[i] * chargingPowers[j + 1] * efficiency[j + 1] + beta * intervalMerge[i] * std::pow(chargingPowers[j + 1] * efficiency[j + 1] - desired[i], 2))
                          - (prices[i] * chargingPowers[j] * efficiency[j] + beta * intervalMerge[i] * std::pow(chargingPowers[j] * efficiency[j] - desired[i], 2)))
                         / (intervalMerge[i] * ((chargingPowers[j + 1] * efficiency[j + 1]) - (chargingPowers[j] * efficiency[j])));
            slopes.push_back({slope, {i, j + 1}});
        }
    }
    return result;
}

Profile OptAlg::bufferPlanning(Profile desired, double targetSoC, double initialSoC, double capacityValue, Profile demand, Profile chargingPowers,
                               double powerMin, double powerMax, Profile powerLimitsLower, Profile powerLimitsUpper,
                               bool reactivePower, Profile prices, double beta, Profile efficiency, Profile intervalMerge) {
    (void)reactivePower; // this C++ port handles active-power profiles only.
    if (prices.empty()) prices = zeros(desired.size());
    if (intervalMerge.empty()) intervalMerge = filled(desired.size(), 1.0);
    if (efficiency.empty()) efficiency = chargingPowers.empty() ? Profile{1.0, 1.0} : filled(chargingPowers.size(), 1.0);
    Profile capacity = filled(desired.size(), capacityValue);
    if (desired.size() != demand.size()) throw std::runtime_error("desired/demand length mismatch");
    if (initialSoC > capacity.front() + 1e-9 || targetSoC > capacity.back() + 1e-9) throw std::runtime_error("invalid SoC/capacity");
    for (double v : demand) if (v < -0.0001) throw std::runtime_error("negative demand not supported");

    bool continuousMode = false;
    if (chargingPowers.empty()) {
        if (!(powerMin < powerMax)) throw std::runtime_error("continuous mode requires powerMin < powerMax");
        chargingPowers = {powerMin, powerMax}; continuousMode = true;
    }
    std::sort(chargingPowers.begin(), chargingPowers.end());

    double demandTotal = 0.0;
    for (size_t i = 0; i < demand.size(); ++i) demandTotal += demand[i] * intervalMerge[i];

    double maxSoC = initialSoC, minSoC = 0.0; int violationIndexMax = -1;
    for (size_t i = 0; i < desired.size(); ++i) {
        if (powerLimitsUpper.size() == desired.size()) maxSoC += std::max(powerLimitsUpper[i], chargingPowers.back() * efficiency.back()) - demand[i] * intervalMerge[i];
        else maxSoC += chargingPowers.back() * efficiency.back() - demand[i] * intervalMerge[i];
        maxSoC = std::min(maxSoC, capacity[i]);
        if (maxSoC < minSoC) { violationIndexMax = static_cast<int>(i); minSoC = maxSoC; }
    }

    // If infeasible, return maximal charging around the violating part; this follows the intent of the Python code without complex recursion corner cases.
    if (violationIndexMax > 0) return filled(desired.size(), chargingPowers.back());

    Profile naivePlan;
    if (continuousMode) {
        naivePlan = continuousBufferPlanning(desired, targetSoC + demandTotal - initialSoC, powerMin, powerMax, powerLimitsLower, powerLimitsUpper, prices, beta);
    } else {
        naivePlan = discreteBufferPlanningPositive(desired, targetSoC + demandTotal - initialSoC, chargingPowers, powerLimitsUpper, prices, beta, efficiency, intervalMerge);
    }

    int violationIndex = -1; double violationAtIndex = 0.01; bool upperBound = false; double soc = initialSoC;
    for (size_t i = 0; i + 1 < desired.size(); ++i) {
        soc += naivePlan[i] * intervalMerge[i] - demand[i] * intervalMerge[i];
        if (soc - capacity[i] > violationAtIndex) { violationIndex = static_cast<int>(i); violationAtIndex = soc - capacity[i]; upperBound = true; }
        else if (-soc > violationAtIndex) { violationIndex = static_cast<int>(i); violationAtIndex = -soc; upperBound = false; }
    }

    if (violationIndex > -1) {
        // Recursive split, matching the Python algorithm's active-power cases.
        size_t cut = static_cast<size_t>(violationIndex) + 1;
        try {
            Profile first, last;
            if (upperBound) {
                first = bufferPlanning(slice(desired, 0, cut), capacity[violationIndex], initialSoC, capacityValue, slice(demand, 0, cut), continuousMode ? Profile{} : chargingPowers, powerMin, powerMax, slice(powerLimitsLower, 0, std::min(cut, powerLimitsLower.size())), slice(powerLimitsUpper, 0, std::min(cut, powerLimitsUpper.size())), false, slice(prices, 0, cut), beta, efficiency, slice(intervalMerge, 0, cut));
                last = bufferPlanning(slice(desired, cut, desired.size()), targetSoC, capacity[violationIndex], capacityValue, slice(demand, cut, demand.size()), continuousMode ? Profile{} : chargingPowers, powerMin, powerMax, powerLimitsLower.size()==desired.size()?slice(powerLimitsLower, cut, powerLimitsLower.size()):Profile{}, powerLimitsUpper.size()==desired.size()?slice(powerLimitsUpper, cut, powerLimitsUpper.size()):Profile{}, false, slice(prices, cut, prices.size()), beta, efficiency, slice(intervalMerge, cut, intervalMerge.size()));
            } else {
                first = bufferPlanning(slice(desired, 0, cut), 0.0, initialSoC, capacityValue, slice(demand, 0, cut), continuousMode ? Profile{} : chargingPowers, powerMin, powerMax, powerLimitsLower.size()==desired.size()?slice(powerLimitsLower, 0, cut):Profile{}, powerLimitsUpper.size()==desired.size()?slice(powerLimitsUpper, 0, cut):Profile{}, false, slice(prices, 0, cut), beta, efficiency, slice(intervalMerge, 0, cut));
                last = bufferPlanning(slice(desired, cut, desired.size()), targetSoC, 0.0, capacityValue, slice(demand, cut, demand.size()), continuousMode ? Profile{} : chargingPowers, powerMin, powerMax, powerLimitsLower.size()==desired.size()?slice(powerLimitsLower, cut, powerLimitsLower.size()):Profile{}, powerLimitsUpper.size()==desired.size()?slice(powerLimitsUpper, cut, powerLimitsUpper.size()):Profile{}, false, slice(prices, cut, prices.size()), beta, efficiency, slice(intervalMerge, cut, intervalMerge.size()));
            }
            first.insert(first.end(), last.begin(), last.end());
            return first;
        } catch (...) { return naivePlan; }
    }
    return naivePlan;
}
