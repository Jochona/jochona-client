#pragma once

#include <QObject>

class TestHostCapabilities : public QObject
{
    Q_OBJECT

private slots:
    void parsesCompatibleManifestAndSelectsExactTuple();
    void rejectsIdentityMismatch();
    void preservesBaselineForUnknownMajor();
    void cacheRoundTripPreservesManifest();
    void mergeNeverRegressesConfirmedOnTransientFailure();
    void mergeNeverRegressesConfirmedOnPartialProbe();
    void mergeAcceptsFreshConfirmedOverConfirmed();
};
