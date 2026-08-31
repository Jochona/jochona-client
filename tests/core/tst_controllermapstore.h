#pragma once

#include <QObject>

class TestControllerMapStore : public QObject
{
    Q_OBJECT

private slots:
    void layersSparseMapsInDeclaredOrder();
    void saveAndResetUseExplicitScopes();
    void roundTripsControllerTransmissionMode();
    void preservesRawPassthroughAndTransmissionModeAcrossLayers();
    void compatibleTransmissionMasksToGenericPad();
    void nativeTransmissionLeavesReportedProfileUnchanged();
};
