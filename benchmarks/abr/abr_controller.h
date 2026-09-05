#pragma once

namespace jochona::abr {

struct Limits {
    int minimumKbps;
    int maximumKbps;
    int initialKbps;
};

struct Observation {
    double intervalSeconds;
    double deliveredKbps;
    double packetLossFraction;
    double rttMs;
    double rttVarianceMs;
    double decoderQueueMs;
    double renderLateFraction;
    bool routeChanged;
};

class Controller {
public:
    explicit Controller(Limits limits);

    int targetKbps() const;
    int update(const Observation& observation);

private:
    void setTarget(int targetKbps);

    Limits m_Limits;
    int m_TargetKbps;
    double m_BaselineRttMs;
    double m_SecondsSinceChange;
    double m_ClearSeconds;
    double m_CongestedSeconds;
};

} // namespace jochona::abr
