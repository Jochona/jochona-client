#include "abr_controller.h"

#include <algorithm>

namespace jochona::abr {

Controller::Controller(Limits limits)
    : m_Limits(limits)
    , m_TargetKbps(std::clamp(limits.initialKbps, limits.minimumKbps, limits.maximumKbps))
{
}

int Controller::targetKbps() const
{
    return m_TargetKbps;
}

int Controller::update(const Observation& observation)
{
    // Jochona's current behavior is a fixed launch bitrate. This intentionally
    // forms the benchmark baseline that a future runtime controller must beat.
    (void) observation;
    return std::clamp(m_TargetKbps, m_Limits.minimumKbps, m_Limits.maximumKbps);
}

} // namespace jochona::abr
