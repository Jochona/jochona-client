#include "abr_controller.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <vector>

namespace {

constexpr double kStepSeconds = 0.25;
constexpr int kMinimumBitrateKbps = 3000;
constexpr int kMaximumBitrateKbps = 60000;
constexpr int kInitialBitrateKbps = 20000;

using jochona::abr::Controller;
using jochona::abr::Observation;

enum class TraceKind {
    StableLan,
    CongestedWan,
    BurstyWifi,
    RouteHandoff,
    CapacityRamp,
    CompetingBulk,
    FloorPressure,
    DecoderPressure,
};

struct Scenario {
    const char* metricName;
    TraceKind kind;
    int durationSeconds;
    int minimumBitrateKbps;
    double usefulQualityCeilingKbps;
};

struct NetworkPoint {
    double capacityKbps;
    double baseRttMs;
    double naturalLossFraction;
    double jitterMs;
    double decoderCapacityKbps;
    bool routeChanged;
};

struct Totals {
    double weightedCost = 0.0;
    double weightedQuality = 0.0;
    double offeredKilobits = 0.0;
    double lostKilobits = 0.0;
    double targetKbpsSeconds = 0.0;
    double durationSeconds = 0.0;
    double frozenSeconds = 0.0;
    double worstScenarioCost = 0.0;
    int bitrateSwitches = 0;
    std::vector<double> queueDelaySamples;
    std::vector<double> scenarioCosts;
};

double clamp01(double value)
{
    return std::clamp(value, 0.0, 1.0);
}

NetworkPoint tracePoint(TraceKind kind, int step)
{
    const double seconds = step * kStepSeconds;
    NetworkPoint point {20000.0, 30.0, 0.0, 2.0, 60000.0, false};

    switch (kind) {
    case TraceKind::StableLan:
        point = {100000.0, 3.0, 0.0, 0.5, 80000.0, false};
        break;
    case TraceKind::CongestedWan:
        if (seconds < 15.0) {
            point = {35000.0, 28.0, 0.001, 3.0, 60000.0, false};
        }
        else if (seconds < 35.0) {
            point = {7000.0, 45.0, 0.004, 8.0, 60000.0, false};
        }
        else {
            point = {26000.0, 32.0, 0.001, 4.0, 60000.0, false};
        }
        break;
    case TraceKind::BurstyWifi: {
        constexpr std::array<double, 12> capacities {
            30000.0, 24000.0, 9000.0, 18000.0, 7000.0, 28000.0,
            12000.0, 32000.0, 6000.0, 20000.0, 15000.0, 26000.0,
        };
        const int bucket = static_cast<int>(seconds) % static_cast<int>(capacities.size());
        const double capacity = capacities[static_cast<std::size_t>(bucket)];
        const double loss = capacity <= 9000.0 ? 0.012 : 0.002;
        point = {capacity, 12.0, loss, 6.0, 60000.0, false};
        break;
    }
    case TraceKind::RouteHandoff:
        if (seconds < 15.0) {
            point = {22000.0, 24.0, 0.001, 3.0, 60000.0, false};
        }
        else if (seconds < 18.0) {
            point = {1200.0, 160.0, 0.08, 35.0, 60000.0, seconds == 15.0};
        }
        else if (seconds < 35.0) {
            point = {9000.0, 72.0, 0.012, 15.0, 60000.0, false};
        }
        else {
            point = {18000.0, 36.0, 0.003, 6.0, 60000.0, false};
        }
        break;
    case TraceKind::CapacityRamp: {
        const double halfCycle = std::fmod(seconds, 40.0);
        const double ramp = halfCycle <= 20.0 ? halfCycle / 20.0 : (40.0 - halfCycle) / 20.0;
        point = {5000.0 + (35000.0 * ramp), 40.0, 0.002, 5.0, 60000.0, false};
        break;
    }
    case TraceKind::CompetingBulk:
        if (seconds < 10.0 || seconds >= 40.0) {
            point = {30000.0, 28.0, 0.001, 3.0, 60000.0, false};
        }
        else {
            const double competingShare = 0.5 + (0.5 * std::sin(seconds * 0.7));
            point = {7000.0 + (9000.0 * competingShare), 45.0, 0.006, 9.0, 60000.0, false};
        }
        break;
    case TraceKind::FloorPressure:
        point = {5500.0, 55.0, 0.008, 10.0, 60000.0, false};
        break;
    case TraceKind::DecoderPressure:
        if (seconds >= 15.0 && seconds < 30.0) {
            point = {80000.0, 5.0, 0.0, 1.0, 12000.0, false};
        }
        else {
            point = {80000.0, 5.0, 0.0, 1.0, 50000.0, false};
        }
        break;
    }

    return point;
}

double percentile95(std::vector<double> samples)
{
    if (samples.empty()) {
        return 0.0;
    }
    std::sort(samples.begin(), samples.end());
    const std::size_t index = static_cast<std::size_t>(std::ceil(samples.size() * 0.95)) - 1;
    return samples[std::min(index, samples.size() - 1)];
}

bool simulateScenario(const Scenario& scenario, Totals& totals)
{
    constexpr int observationSteps = static_cast<int>(1.0 / kStepSeconds);
    const int initialKbps = std::max(kInitialBitrateKbps, scenario.minimumBitrateKbps);
    Controller controller({scenario.minimumBitrateKbps, kMaximumBitrateKbps, initialKbps});
    std::array<int, 2> actuationDelay {controller.targetKbps(), controller.targetKbps()};

    double networkQueueKilobits = 0.0;
    double decoderQueueKilobits = 0.0;
    double previousQueueDelayMs = 0.0;
    double scenarioCost = 0.0;
    double observedDeliveredKbpsSeconds = 0.0;
    double observedOfferedKilobits = 0.0;
    double observedLostKilobits = 0.0;
    double observedRttMs = 0.0;
    double observedMaxRttVarianceMs = 0.0;
    double observedMaxDecoderQueueMs = 0.0;
    double observedRenderLateSeconds = 0.0;
    bool observedRouteChange = false;
    int previousCommandKbps = controller.targetKbps();
    const int steps = static_cast<int>(scenario.durationSeconds / kStepSeconds);

    for (int step = 0; step < steps; ++step) {
        const NetworkPoint network = tracePoint(scenario.kind, step);
        const int appliedTargetKbps = actuationDelay.front();
        actuationDelay.front() = actuationDelay.back();

        const double offeredKilobits = appliedTargetKbps * kStepSeconds;
        const double naturalLostKilobits = offeredKilobits * network.naturalLossFraction;
        const double admittedKilobits = offeredKilobits - naturalLostKilobits;
        const double serviceKilobits = network.capacityKbps * kStepSeconds;
        const double availableKilobits = networkQueueKilobits + admittedKilobits;
        const double deliveredKilobits = std::min(availableKilobits, serviceKilobits);
        networkQueueKilobits = availableKilobits - deliveredKilobits;

        const double maximumQueueKilobits = network.capacityKbps * 0.6;
        const double overflowKilobits = std::max(0.0, networkQueueKilobits - maximumQueueKilobits);
        networkQueueKilobits -= overflowKilobits;

        const double packetLossFraction = offeredKilobits > 0.0
            ? clamp01((naturalLostKilobits + overflowKilobits) / offeredKilobits)
            : 0.0;
        const double queueDelayMs = network.capacityKbps > 0.0
            ? networkQueueKilobits * 1000.0 / network.capacityKbps
            : 600.0;
        const double deterministicJitterMs = network.jitterMs * std::abs(std::sin(step * 1.61803398875));
        const double rttMs = network.baseRttMs + queueDelayMs + deterministicJitterMs;
        const double rttVarianceMs = network.jitterMs + std::abs(queueDelayMs - previousQueueDelayMs);
        previousQueueDelayMs = queueDelayMs;

        decoderQueueKilobits += deliveredKilobits;
        const double decoderServiceKilobits = network.decoderCapacityKbps * kStepSeconds;
        decoderQueueKilobits = std::max(0.0, decoderQueueKilobits - decoderServiceKilobits);
        const double maximumDecoderQueueKilobits = network.decoderCapacityKbps * 0.5;
        const double decoderOverflowKilobits = std::max(0.0, decoderQueueKilobits - maximumDecoderQueueKilobits);
        decoderQueueKilobits -= decoderOverflowKilobits;
        const double decoderQueueMs = decoderQueueKilobits * 1000.0 / network.decoderCapacityKbps;

        const double networkLateFraction = clamp01((queueDelayMs - 50.0) / 200.0);
        const double decoderLateFraction = clamp01((decoderQueueMs - 33.0) / 167.0);
        const double decoderOverflowFraction = deliveredKilobits > 0.0
            ? clamp01(decoderOverflowKilobits / deliveredKilobits)
            : 0.0;
        const double renderLateFraction = clamp01(
            std::max(networkLateFraction, decoderLateFraction)
            + (packetLossFraction * 1.5)
            + decoderOverflowFraction);

        const double deliveredKbps = deliveredKilobits / kStepSeconds;
        const double effectiveQualityKbps = std::min<double>(appliedTargetKbps, deliveredKbps)
            * (1.0 - renderLateFraction);
        const double quality = clamp01(
            std::log1p(effectiveQualityKbps / 500.0)
            / std::log1p(scenario.usefulQualityCeilingKbps / 500.0));

        const double qualityCost = 1.0 - quality;
        const double lateCost = renderLateFraction * 15.0;
        const double lossCost = packetLossFraction * 20.0;
        const double queueCost = std::max(0.0, queueDelayMs - 20.0) * 0.02;
        const double decoderCost = std::max(0.0, decoderQueueMs - 30.0) * 0.015;
        scenarioCost += (qualityCost + lateCost + lossCost + queueCost + decoderCost) * kStepSeconds;

        totals.weightedQuality += quality * kStepSeconds;
        totals.offeredKilobits += offeredKilobits;
        totals.lostKilobits += naturalLostKilobits + overflowKilobits;
        totals.targetKbpsSeconds += appliedTargetKbps * kStepSeconds;
        totals.durationSeconds += kStepSeconds;
        totals.frozenSeconds += renderLateFraction >= 0.75 ? kStepSeconds : 0.0;
        totals.queueDelaySamples.push_back(queueDelayMs);

        observedDeliveredKbpsSeconds += deliveredKbps * kStepSeconds;
        observedOfferedKilobits += offeredKilobits;
        observedLostKilobits += naturalLostKilobits + overflowKilobits;
        observedRttMs += rttMs;
        observedMaxRttVarianceMs = std::max(observedMaxRttVarianceMs, rttVarianceMs);
        observedMaxDecoderQueueMs = std::max(observedMaxDecoderQueueMs, decoderQueueMs);
        observedRenderLateSeconds += renderLateFraction * kStepSeconds;
        observedRouteChange = observedRouteChange || network.routeChanged;

        if ((step + 1) % observationSteps == 0) {
            Observation observation {
                observationSteps * kStepSeconds,
                observedDeliveredKbpsSeconds / (observationSteps * kStepSeconds),
                observedOfferedKilobits > 0.0
                    ? clamp01(observedLostKilobits / observedOfferedKilobits)
                    : 0.0,
                observedRttMs / observationSteps,
                observedMaxRttVarianceMs,
                observedMaxDecoderQueueMs,
                observedRenderLateSeconds / (observationSteps * kStepSeconds),
                observedRouteChange,
            };
            const int commandedTargetKbps = controller.update(observation);
            if (commandedTargetKbps < scenario.minimumBitrateKbps
                || commandedTargetKbps > kMaximumBitrateKbps) {
                std::cerr << "controller target outside configured limits\n";
                return false;
            }

            if (commandedTargetKbps != previousCommandKbps) {
                const double ratio = static_cast<double>(commandedTargetKbps) / previousCommandKbps;
                scenarioCost += std::abs(std::log2(ratio)) * 0.35;
                ++totals.bitrateSwitches;
                previousCommandKbps = commandedTargetKbps;
            }

            observedDeliveredKbpsSeconds = 0.0;
            observedOfferedKilobits = 0.0;
            observedLostKilobits = 0.0;
            observedRttMs = 0.0;
            observedMaxRttVarianceMs = 0.0;
            observedMaxDecoderQueueMs = 0.0;
            observedRenderLateSeconds = 0.0;
            observedRouteChange = false;
        }
        actuationDelay.back() = previousCommandKbps;
    }

    const double normalizedScenarioCost = scenarioCost / scenario.durationSeconds;
    totals.weightedCost += normalizedScenarioCost;
    totals.worstScenarioCost = std::max(totals.worstScenarioCost, normalizedScenarioCost);
    totals.scenarioCosts.push_back(normalizedScenarioCost);
    return true;
}

} // namespace

int main()
{
    constexpr std::array<Scenario, 8> scenarios {{
        {"stable_lan_cost", TraceKind::StableLan, 45, kMinimumBitrateKbps, 50000.0},
        {"congested_wan_cost", TraceKind::CongestedWan, 60, kMinimumBitrateKbps, 30000.0},
        {"bursty_wifi_cost", TraceKind::BurstyWifi, 60, kMinimumBitrateKbps, 30000.0},
        {"route_handoff_cost", TraceKind::RouteHandoff, 55, kMinimumBitrateKbps, 22000.0},
        {"capacity_ramp_cost", TraceKind::CapacityRamp, 60, kMinimumBitrateKbps, 40000.0},
        {"competing_bulk_cost", TraceKind::CompetingBulk, 55, kMinimumBitrateKbps, 30000.0},
        {"floor_pressure_cost", TraceKind::FloorPressure, 35, 12000, 12000.0},
        {"decoder_pressure_cost", TraceKind::DecoderPressure, 45, kMinimumBitrateKbps, 50000.0},
    }};

    Totals totals;
    totals.queueDelaySamples.reserve(1800);
    totals.scenarioCosts.reserve(scenarios.size());
    for (const Scenario& scenario : scenarios) {
        if (!simulateScenario(scenario, totals)) {
            return EXIT_FAILURE;
        }
    }

    const double meanScenarioCost = totals.weightedCost / scenarios.size();
    const double qoeCost = meanScenarioCost + (totals.worstScenarioCost * 0.25);
    const double meanQualityPct = totals.weightedQuality * 100.0 / totals.durationSeconds;
    const double networkLossPct = totals.lostKilobits * 100.0 / totals.offeredKilobits;
    const double frozenTimePct = totals.frozenSeconds * 100.0 / totals.durationSeconds;
    const double meanTargetKbps = totals.targetKbpsSeconds / totals.durationSeconds;

    std::cout << std::fixed << std::setprecision(6)
              << "METRIC qoe_cost=" << qoeCost << '\n'
              << "METRIC worst_scenario_cost=" << totals.worstScenarioCost << '\n'
              << "METRIC mean_quality_pct=" << meanQualityPct << '\n'
              << "METRIC network_loss_pct=" << networkLossPct << '\n'
              << "METRIC frozen_time_pct=" << frozenTimePct << '\n'
              << "METRIC p95_queue_delay_ms=" << percentile95(totals.queueDelaySamples) << '\n'
              << "METRIC mean_target_kbps=" << meanTargetKbps << '\n'
              << "METRIC bitrate_switches=" << totals.bitrateSwitches << '\n';
    for (std::size_t i = 0; i < scenarios.size(); ++i) {
        std::cout << "METRIC " << scenarios[i].metricName << '='
                  << totals.scenarioCosts[i] << '\n';
    }
    return EXIT_SUCCESS;
}
