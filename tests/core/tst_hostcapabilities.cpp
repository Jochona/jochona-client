#include "tst_hostcapabilities.h"

#include "backend/adapters/hostcapabilities.h"

#include <Limelight.h>
#include <QJsonArray>
#include <QJsonObject>
#include <QtTest>
#include <functional>

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
             QStringLiteral("host.volume.read"),
             QStringLiteral("host.volume.write"),
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
                 {QStringLiteral("proof"), QJsonObject{
                      {QStringLiteral("method"), QStringLiteral("vendor-query+probe-frames")},
                      {QStringLiteral("gpu"), QStringLiteral("10de:2684")},
                      {QStringLiteral("driver"), QStringLiteral("566.03")},
                      {QStringLiteral("displayMode"), QStringLiteral("3840x2160@120-hdr")},
                      {QStringLiteral("hostBuild"), QStringLiteral("0.1.0")},
                      {QStringLiteral("verifiedAt"), QStringLiteral("2026-08-27T00:00:00Z")},
                  }},
             },
             QJsonObject{
                 {QStringLiteral("id"), QStringLiteral("amf-h264-high-420-1080p60-sdr")},
                 {QStringLiteral("codec"), QStringLiteral("h264")},
                 {QStringLiteral("profile"), QStringLiteral("main8")},
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
                 {QStringLiteral("proof"), QJsonObject{
                      {QStringLiteral("method"), QStringLiteral("vendor-query+probe-frames")},
                      {QStringLiteral("gpu"), QStringLiteral("1002:73df")},
                      {QStringLiteral("driver"), QStringLiteral("24.20.1")},
                      {QStringLiteral("displayMode"), QStringLiteral("1920x1080@60-sdr")},
                      {QStringLiteral("hostBuild"), QStringLiteral("0.1.0")},
                      {QStringLiteral("verifiedAt"), QStringLiteral("2026-08-27T00:00:00Z")},
                  }},
             },
         }},
        {QStringLiteral("virtualDisplay"), QJsonObject{
             {QStringLiteral("installed"), true},
             {QStringLiteral("healthy"), true},
             {QStringLiteral("pool"), QJsonArray{}},
         }},
        {QStringLiteral("runtimeControls"), QJsonObject{
             {QStringLiteral("hostVolume"), QJsonObject{
                  {QStringLiteral("available"), true},
                  {QStringLiteral("min"), 0},
                  {QStringLiteral("max"), 100},
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
    QVERIFY(capabilities.hasCapability(HostCapabilities::VolumeControl));
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

namespace
{
// Returns manifest()'s first encoder tuple with the given mutation applied
// via fn(QJsonObject& tuple), leaving the second tuple and every other
// manifest field untouched.
QJsonObject manifestWithMutatedFirstTuple(
        const std::function<void(QJsonObject&)>& fn)
{
    QJsonObject m = manifest();
    QJsonArray tuples = m.value(QStringLiteral("encoderTuples")).toArray();
    QJsonObject tuple0 = tuples.at(0).toObject();
    fn(tuple0);
    tuples[0] = tuple0;
    m[QStringLiteral("encoderTuples")] = tuples;
    return m;
}
}

void TestHostCapabilities::rejectsEncoderTupleMissingProof()
{
    HostCapabilities capabilities;
    QString error;
    const QJsonObject malformed = manifestWithMutatedFirstTuple(
        [](QJsonObject& tuple) { tuple.remove(QStringLiteral("proof")); });

    // ADR-0011: a tuple with no proof object at all must not be accepted
    // as compatible -- it is a manifest claiming a codec path exists
    // without ever having run the required vendor-query+probe-frame check.
    QCOMPARE(capabilities.applyJochonaManifest(
                 malformed, QStringLiteral("host-1"), &error),
             HostCapabilities::ManifestStatus::Invalid);
    QVERIFY(!error.isEmpty());
    QVERIFY(capabilities.encoderTuples.isEmpty());
}

void TestHostCapabilities::rejectsEncoderTupleWithIncompleteProof()
{
    HostCapabilities capabilities;
    QString error;
    const QJsonObject malformed = manifestWithMutatedFirstTuple(
        [](QJsonObject& tuple) {
            QJsonObject proof = tuple.value(QStringLiteral("proof")).toObject();
            proof.remove(QStringLiteral("verifiedAt"));
            tuple[QStringLiteral("proof")] = proof;
        });

    // Missing even one of the six documented proof fields (method/gpu/
    // driver/displayMode/hostBuild/verifiedAt) must reject the tuple.
    QCOMPARE(capabilities.applyJochonaManifest(
                 malformed, QStringLiteral("host-1"), &error),
             HostCapabilities::ManifestStatus::Invalid);
    QVERIFY(!error.isEmpty());
}

void TestHostCapabilities::rejectsEncoderTupleWithEmptyProofField()
{
    HostCapabilities capabilities;
    QString error;
    const QJsonObject malformed = manifestWithMutatedFirstTuple(
        [](QJsonObject& tuple) {
            QJsonObject proof = tuple.value(QStringLiteral("proof")).toObject();
            proof[QStringLiteral("gpu")] = QString();
            tuple[QStringLiteral("proof")] = proof;
        });

    // A present-but-empty proof field is exactly as unproven as an absent
    // one and must be rejected the same way.
    QCOMPARE(capabilities.applyJochonaManifest(
                 malformed, QStringLiteral("host-1"), &error),
             HostCapabilities::ManifestStatus::Invalid);
    QVERIFY(!error.isEmpty());
}

void TestHostCapabilities::withholdsVolumeControlWithoutPermission()
{
    HostCapabilities capabilities;
    QJsonObject m = manifest();
    QJsonArray permissions = m.value(QStringLiteral("permissions")).toArray();
    QJsonArray restricted;
    for (const QJsonValue& permission : std::as_const(permissions)) {
        if (permission.toString() != QLatin1String("host.volume.read")) {
            restricted.append(permission);
        }
    }
    m[QStringLiteral("permissions")] = restricted;

    // runtimeControls.hostVolume.available alone must not grant
    // VolumeControl -- this pinned client also needs the canonical
    // host.volume.read permission (permissions are per-client, not global).
    QCOMPARE(capabilities.applyJochonaManifest(m, QStringLiteral("host-1")),
             HostCapabilities::ManifestStatus::Compatible);
    QVERIFY(!capabilities.hasCapability(HostCapabilities::VolumeControl));
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

void TestHostCapabilities::videoFormatWireFieldsRoundTrip()
{
    const QList<int> formats{
        VIDEO_FORMAT_H264,
        VIDEO_FORMAT_H264_HIGH8_444,
        VIDEO_FORMAT_H265,
        VIDEO_FORMAT_H265_MAIN10,
        VIDEO_FORMAT_H265_REXT8_444,
        VIDEO_FORMAT_H265_REXT10_444,
        VIDEO_FORMAT_AV1_MAIN8,
        VIDEO_FORMAT_AV1_MAIN10,
        VIDEO_FORMAT_AV1_HIGH8_444,
        VIDEO_FORMAT_AV1_HIGH10_444,
    };

    for (int format : formats) {
        const HostCapabilities::EncoderTuple tuple =
            HostCapabilities::EncoderTuple::fromVideoFormat(format);
        QVERIFY2(!tuple.codec.isEmpty(), qPrintable(QString::number(format)));
        QCOMPARE(tuple.videoFormat(), format);
        QCOMPARE(tuple.profile,
                 tuple.bitDepth == 10 ? QStringLiteral("main10")
                                      : QStringLiteral("main8"));
    }

    const HostCapabilities::EncoderTuple invalid =
        HostCapabilities::EncoderTuple::fromVideoFormat(
            VIDEO_FORMAT_H264 | VIDEO_FORMAT_H265);
    QVERIFY(invalid.codec.isEmpty());
}

void TestHostCapabilities::rejectsNonCanonicalProfile()
{
    QJsonObject document = manifest();
    QJsonArray tuples =
        document.value(QStringLiteral("encoderTuples")).toArray();
    QJsonObject tuple = tuples.first().toObject();
    tuple.insert(QStringLiteral("profile"), QStringLiteral("high10"));
    tuples.replace(0, tuple);
    document.insert(QStringLiteral("encoderTuples"), tuples);

    HostCapabilities capabilities;
    QString error;
    QCOMPARE(capabilities.applyJochonaManifest(
                 document, QStringLiteral("host-1"), &error),
             HostCapabilities::ManifestStatus::Invalid);
    QVERIFY(!error.isEmpty());
}
