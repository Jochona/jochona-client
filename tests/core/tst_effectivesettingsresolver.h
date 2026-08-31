#pragma once

#include <QObject>

class TestEffectiveSettingsResolver : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void streamingProfileAppliesBeforePairPatches();
    void selectsMostSpecificStreamingProfileScope();
    void profilePinSuppressesAutomaticSwitchAndReportsConflict();
    void capabilitySafetyOverridesPinnedFieldAndReportsConflict();
};
