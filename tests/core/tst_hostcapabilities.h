#pragma once

#include <QObject>

class TestHostCapabilities : public QObject
{
    Q_OBJECT

private slots:
    void parsesCompatibleManifestAndSelectsExactTuple();
    void videoFormatWireFieldsRoundTrip();
    void rejectsNonCanonicalProfile();
    void rejectsIdentityMismatch();
    void preservesBaselineForUnknownMajor();
    void cacheRoundTripPreservesManifest();
    void mergeNeverRegressesConfirmedOnTransientFailure();
    void mergeNeverRegressesConfirmedOnPartialProbe();
    void mergeAcceptsFreshConfirmedOverConfirmed();
    void rejectsEncoderTupleMissingProof();
    void rejectsEncoderTupleWithIncompleteProof();
    void rejectsEncoderTupleWithEmptyProofField();
    void withholdsVolumeControlWithoutPermission();
};
