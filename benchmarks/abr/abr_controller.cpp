#include "abr_controller.h"

#include <algorithm>
#include <cmath>

namespace jochona::abr {

Controller::Controller(Limits limits)
    : m_Limits(limits)
    , m_TargetKbps(std::clamp(limits.initialKbps, limits.minimumKbps, limits.maximumKbps))
    , m_BaselineRttMs(0.0)
    , m_SecondsSinceChange(0.0)
    , m_ClearSeconds(0.0)
    , m_CongestedSeconds(0.0)
{
}

int Controller::targetKbps() const
{
    return m_TargetKbps;
}
void Controller::setTarget(int targetKbps)
{
    const int clamped = std::clamp(targetKbps, m_Limits.minimumKbps, m_Limits.maximumKbps);
    if (clamped != m_TargetKbps) {
        m_TargetKbps = clamped;
        m_SecondsSinceChange = 0.0;
    }
}

int Controller::update(const Observation& observation)
{
    const double elapsed = std::max(0.1, observation.intervalSeconds);
    m_SecondsSinceChange += elapsed;

    if (m_BaselineRttMs <= 0.0 || observation.rttMs < m_BaselineRttMs) {
        m_BaselineRttMs = observation.rttMs;
    }

    if (observation.routeChanged) {
        m_BaselineRttMs = observation.rttMs;
        m_ClearSeconds = 0.0;
        m_CongestedSeconds = 0.0;
        setTarget(static_cast<int>(std::lround(m_TargetKbps * 0.70)));
        return m_TargetKbps;
    }

    const double queueDelayMs = std::max(0.0, observation.rttMs - m_BaselineRttMs);
    const bool severeCongestion =
        observation.packetLossFraction > 0.10
        || observation.renderLateFraction > 0.20
        || observation.decoderQueueMs > 120.0
        || queueDelayMs > 120.0;
    const bool congestion =
        severeCongestion
        || observation.packetLossFraction > 0.02
        || observation.renderLateFraction > 0.05
        || observation.decoderQueueMs > 60.0
        || queueDelayMs > 50.0
        || observation.rttVarianceMs > 35.0;

    if (congestion) {
        m_CongestedSeconds += elapsed;
        m_ClearSeconds = 0.0;
    }
    else {
        m_ClearSeconds += elapsed;
        m_CongestedSeconds = std::max(0.0, m_CongestedSeconds - elapsed);
    }

    if (severeCongestion && m_SecondsSinceChange >= 1.0) {
        const double achievedKbps = observation.deliveredKbps > 0.0
            ? std::min<double>(m_TargetKbps, observation.deliveredKbps)
            : m_TargetKbps;
        setTarget(static_cast<int>(std::lround(
            std::min(m_TargetKbps * 0.80, achievedKbps * 0.55))));
        m_CongestedSeconds = 0.0;
    }
    else if (m_CongestedSeconds >= 2.0 && m_SecondsSinceChange >= 2.0) {
        const double achievedKbps = observation.deliveredKbps > 0.0
            ? std::min<double>(m_TargetKbps, observation.deliveredKbps)
            : m_TargetKbps;
        setTarget(static_cast<int>(std::lround(
            std::min(m_TargetKbps * 0.85, achievedKbps * 0.90))));
        m_CongestedSeconds = 0.0;
    }
    else if (m_ClearSeconds >= 5.0 && m_SecondsSinceChange >= 5.0) {
        setTarget(m_TargetKbps + 1000);
        m_ClearSeconds = 0.0;
    }

    return m_TargetKbps;
}

} // namespace jochona::abr
