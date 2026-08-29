#include "tst_hostcapabilities.h"

#include "backend/adapters/hostcapabilities.h"

#include <Limelight.h>
#include <QJsonArray>
#include <QJsonObject>
#include <QtTest>

namespace
{
QJsonObject manifest(int major = 1,
                     const QString& identity = QStringLiteral("host-1"))
{
    return {
        {QStringLiteral("schema"), QJsonObject{
             {QStringLiteral("major"), major},
             {QStringLiteral("minor"), 0},
         }},
        {QStringLiteral("host"), QJsonObject{
             {QStringLiteral("software"), QStringLiteral("Jochona Host")},
             {QStringLiteral("build"), QStringLiteral("0.1.0")},
             {QStringLiteral("identity"), identity},
             {QStringLiteral("capacity"), QJsonObject{
                  {QStringLiteral("state"), QStringLiteral("ready")},
                  {QStringLiteral("maxSessions"), 1},
                  {QStringLiteral("activeApplication"), QJsonValue()},
              }},
         }},
        {QStringLiteral("permissions"), QJsonArray{
             QStringLiteral("session.launch"),
             QStringLiteral("session.stop"),
         }},
        {QStringLiteral("encoderTuples"), QJsonArray{
             QJsonObject{
                 {QStringLiteral("id"), QStringLiteral("nvenc-av1-main10-420-4k120-hdr")},
                 {QStringLiteral("codec"), QStringLiteral("av1")},
                 {QStringLiteral("profile"), QStringLiteral("main10")},
                 {QStringLiteral("bitDepth"), 10},
                 {QStringLiteral("chroma"), QStringLiteral("420")},
                 {QStringLiteral("width"), 3840},
                 {QStringLiteral("height"), 2160},
                 {QStringLiteral("fps"), 120},
                 {QStringLiteral("hdr"), QJsonObject{
                      {QStringLiteral("supported"), true},
                  }},
                 {QStringLiteral("capture"), QJsonArray{
                      QStringLiteral("physical"),
                      QStringLiteral("virtual"),
                  }},
             },
             QJsonObject{
                 {QStringLiteral("id"), QStringLiteral("amf-h264-high-420-1080p60-sdr")},
                 {QStringLiteral("codec"), QStringLiteral("h264")},
                 {QStringLiteral("profile"), QStringLiteral("high")},
                 {QStringLiteral("bitDepth"), 8},
                 {QStringLiteral("chroma"), QStringLiteral("420")},
                 {QStringLiteral("width"), 1920},
                 {QStringLiteral("height"), 1080},
                 {QStringLiteral("fps"), 60},
                 {QStringLiteral("hdr"), QJsonObject{
                      {QStringLiteral("supported"), false},
                  }},
                 {QStringLiteral("capture"), QJsonArray{
                      QStringLiteral("physical"),
                  }},
             },
         }},
        {QStringLiteral("virtualDisplay"), QJsonObject{
             {QStringLiteral("installed"), true},
             {QStringLiteral("healthy"), true},
         }},
        {QStringLiteral("runtimeControls"), QJsonObject{
             {QStringLiteral("hostVolume"), QJsonObject{
                  {QStringLiteral("available"), true},
              }},
         }},
    };
}
}

void TestHostCapabilities::parsesCompatibleManifestAndSelectsExactTuple()
{
    HostCapabilities capabilities;
    QString error;

    QCOMPARE(capabilities.applyJochonaManifest(
                 manifest(), QStringLiteral("HOST-1"), &error),
             HostCapabilities::ManifestStatus::Compatible);
    QVERIFY(error.isEmpty());
    QCOMPARE(capabilities.family, HostCapabilities::Family::Jochona);
    QVERIFY(capabilities.hasCapability(HostCapabilities::JochonaManifest));
    QVERIFY(capabilities.hasCapability(
                HostCapabilities::VirtualDisplayDriverReady));
    QVERIFY(capabilities.allowLaunch());

    const QList<int> preferred{
        VIDEO_FORMAT_AV1_MAIN10,
        VIDEO_FORMAT_H265_MAIN10,
        VIDEO_FORMAT_H264,
    };
    QCOMPARE(capabilities.selectEncoderTuple(
                 3840, 2160, 120, preferred, true, true),
             QStringLiteral("nvenc-av1-main10-420-4k120-hdr"));
    QVERIFY(capabilities.selectEncoderTuple(
                1920, 1080, 60, preferred, false, true).isEmpty());
    QCOMPARE(capabilities.selectEncoderTuple(
                 1920, 1080, 60, preferred, false, false),
             QStringLiteral("amf-h264-high-420-1080p60-sdr"));
}

void TestHostCapabilities::rejectsIdentityMismatch()
{
    HostCapabilities capabilities;
    QString error;

    QCOMPARE(capabilities.applyJochonaManifest(
                 manifest(), QStringLiteral("another-host"), &error),
             HostCapabilities::ManifestStatus::Invalid);
    QVERIFY(!error.isEmpty());
    QVERIFY(!capabilities.hasCapability(HostCapabilities::JochonaManifest));
}

void TestHostCapabilities::preservesBaselineForUnknownMajor()
{
    HostCapabilities capabilities;

    QCOMPARE(capabilities.applyJochonaManifest(
                 manifest(2), QStringLiteral("host-1")),
             HostCapabilities::ManifestStatus::Incompatible);
    QCOMPARE(capabilities.family, HostCapabilities::Family::Jochona);
    QVERIFY(!capabilities.hasCapability(HostCapabilities::JochonaManifest));
    QVERIFY(capabilities.selectEncoderTuple(
                3840, 2160, 120,
                {VIDEO_FORMAT_AV1_MAIN10}, true, false).isEmpty());
}

void TestHostCapabilities::cacheRoundTripPreservesManifest()
{
    HostCapabilities capabilities;
    QCOMPARE(capabilities.applyJochonaManifest(
                 manifest(), QStringLiteral("host-1")),
             HostCapabilities::ManifestStatus::Compatible);
    capabilities.confidence = HostCapabilities::Confidence::Confirmed;

    const HostCapabilities restored =
        HostCapabilities::fromJson(capabilities.toJson());
    QCOMPARE(restored, capabilities);
}

void TestHostCapabilities::mergeNeverRegressesConfirmedOnTransientFailure()
{
    HostCapabilities confirmed;
    QCOMPARE(confirmed.applyJochonaManifest(
                 manifest(), QStringLiteral("host-1")),
             HostCapabilities::ManifestStatus::Compatible);
    confirmed.confidence = HostCapabilities::Confidence::Confirmed;

    // A Host that briefly could not be reached (asleep, Wi-Fi hiccup)
    // reports a bare Confidence::Unknown result -- it must never wipe out
    // an already-Confirmed manifest and encoder tuple set.
    const HostCapabilities unreachable;
    QCOMPARE(unreachable.confidence, HostCapabilities::Confidence::Unknown);

    const HostCapabilities merged =
        HostCapabilities::mergeProbeResult(confirmed, unreachable);
    QCOMPARE(merged, confirmed);
    QCOMPARE(merged.manifestStatus, HostCapabilities::ManifestStatus::Compatible);
    QVERIFY(!merged.encoderTuples.isEmpty());
}

void TestHostCapabilities::mergeNeverRegressesConfirmedOnPartialProbe()
{
    HostCapabilities confirmed;
    QCOMPARE(confirmed.applyJochonaManifest(
                 manifest(), QStringLiteral("host-1")),
             HostCapabilities::ManifestStatus::Compatible);
    confirmed.confidence = HostCapabilities::Confidence::Confirmed;

    HostCapabilities partial;
    partial.family = HostCapabilities::Family::Apollo;
    partial.confidence = HostCapabilities::Confidence::Partial;

    const HostCapabilities merged =
        HostCapabilities::mergeProbeResult(confirmed, partial);
    QCOMPARE(merged, confirmed);
}

void TestHostCapabilities::mergeAcceptsFreshConfirmedOverConfirmed()
{
    HostCapabilities stale;
    QCOMPARE(stale.applyJochonaManifest(
                 manifest(), QStringLiteral("host-1")),
             HostCapabilities::ManifestStatus::Compatible);
    stale.confidence = HostCapabilities::Confidence::Confirmed;

    // A second, successful probe run is trusted outright even though it
    // is also Confirmed -- e.g. the Host's encoder tuple set changed.
    HostCapabilities fresh;
    QCOMPARE(fresh.applyJochonaManifest(
                 manifest(2), QStringLiteral("host-1")),
             HostCapabilities::ManifestStatus::Incompatible);
    fresh.confidence = HostCapabilities::Confidence::Confirmed;

    const HostCapabilities merged =
        HostCapabilities::mergeProbeResult(stale, fresh);
    QCOMPARE(merged, fresh);
    QCOMPARE(merged.manifestStatus, HostCapabilities::ManifestStatus::Incompatible);
}
