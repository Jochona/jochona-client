#include "tst_effectivesettingsresolver.h"

#include "core/settingsdatabase.h"
#include "settings/effectivesettingsresolver.h"

#include <QTemporaryDir>
#include <QTest>


void TestEffectiveSettingsResolver::initTestCase()
{
    EffectiveSettingsResolver::setBaselineProvider([]() {
        return QVariantMap{
            {QStringLiteral("width"), 1280},
            {QStringLiteral("height"), 720},
            {QStringLiteral("fps"), 60},
            {QStringLiteral("bitrate"), 10000},
            {QStringLiteral("videocfg"), 0},
            {QStringLiteral("gameopts"), true},
            {QStringLiteral("packetsize"), 1024},
        };
    });
    EffectiveSettingsResolver::setCapabilitySafetyProvider(
        [](const QString&, const QVariantMap& candidate) {
            QVariantMap safe = candidate;
            QVariantMap reasons;
            if (safe.value(QStringLiteral("width")).toInt() > 1920) {
                safe.insert(QStringLiteral("width"), 1920);
                reasons.insert(QStringLiteral("width"),
                               QStringLiteral("Test display width limit"));
            }
            if (safe.value(QStringLiteral("height")).toInt() > 1080) {
                safe.insert(QStringLiteral("height"), 1080);
                reasons.insert(QStringLiteral("height"),
                               QStringLiteral("Test display height limit"));
            }
            safe.insert(QStringLiteral("reasons"), reasons);
            return safe;
        });
}

void TestEffectiveSettingsResolver::cleanupTestCase()
{
    EffectiveSettingsResolver::setBaselineProvider({});
    EffectiveSettingsResolver::setCapabilitySafetyProvider({});
}
// The Streaming Profile layer must apply before the Host-Client Pair /
// Library Entry / Host Application patches (Baseline -> Streaming Profile
// -> Pair/Library/Application patches -> Session Patch -> Capability
// Safety), so a Pair patch can still override an individual field the
// Profile also supplies, while fields the patch never mentions keep the
// Profile's value.
void TestEffectiveSettingsResolver::streamingProfileAppliesBeforePairPatches()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SettingsDatabase db;
    QVERIFY2(db.open(dir.filePath(QStringLiteral("jochona.db"))),
             qPrintable(db.lastError()));

    EffectiveSettingsResolver* resolver = EffectiveSettingsResolver::get();
    const QString clientDeviceId = QStringLiteral("precedence-device");
    const QString hostUuid = QStringLiteral("precedence-host");

    const QString profileId = resolver->saveStreamingProfile(
        QStringLiteral("Living Room"),
        QVariantMap{
            {QStringLiteral("packetsize"), 1200},
            {QStringLiteral("gameopts"), false},
        });
    QVERIFY(!profileId.isEmpty());
    QVERIFY(resolver->bindStreamingProfileScope(
        profileId, QStringLiteral("client_device"), clientDeviceId));

    QVERIFY(resolver->setPatch(
        QStringLiteral("host_client_pair"),
        clientDeviceId + QStringLiteral("|") + hostUuid,
        QVariantMap{{QStringLiteral("packetsize"), 1400}}));

    const QVariantMap resolved = resolver->resolve(QVariantMap{
        {QStringLiteral("clientDeviceId"), clientDeviceId},
        {QStringLiteral("hostUuid"), hostUuid},
    });
    const QVariantMap values = resolved.value(QStringLiteral("values")).toMap();
    const QVariantMap provenance =
        resolved.value(QStringLiteral("provenance")).toMap();

    // The Pair patch (more specific) overrides the Profile's packetsize.
    QCOMPARE(values.value(QStringLiteral("packetsize")).toInt(), 1400);
    QCOMPARE(provenance.value(QStringLiteral("packetsize")).toMap()
                 .value(QStringLiteral("source")).toString(),
             QStringLiteral("Host\u2013Client Pair"));

    // gameopts is untouched by the patch, so it keeps the Profile's value
    // (false), not the Settings Baseline default (true).
    QCOMPARE(values.value(QStringLiteral("gameopts")).toBool(), false);
    QCOMPARE(provenance.value(QStringLiteral("gameopts")).toMap()
                 .value(QStringLiteral("source")).toString(),
             QStringLiteral("Streaming Profile"));
}

// Deterministic most-specific Streaming Profile selection across Client
// Device, Library Entry, and Host Application scopes: the most specific
// bound scope wins, and removing it falls back to the next most specific.
void TestEffectiveSettingsResolver::selectsMostSpecificStreamingProfileScope()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SettingsDatabase db;
    QVERIFY2(db.open(dir.filePath(QStringLiteral("jochona.db"))),
             qPrintable(db.lastError()));

    EffectiveSettingsResolver* resolver = EffectiveSettingsResolver::get();
    const QString clientDeviceId = QStringLiteral("scope-device");
    const QString hostUuid = QStringLiteral("scope-host");
    const QString libraryEntryId = QStringLiteral("scope-entry");
    const QString appId = QStringLiteral("77");
    const QString hostApplicationKey =
        hostUuid + QStringLiteral("|") + appId;

    const QString deviceProfile = resolver->saveStreamingProfile(
        QStringLiteral("Device Default"),
        QVariantMap{{QStringLiteral("fps"), 30}});
    const QString libraryProfile = resolver->saveStreamingProfile(
        QStringLiteral("Library Default"),
        QVariantMap{{QStringLiteral("fps"), 60}});
    const QString appProfile = resolver->saveStreamingProfile(
        QStringLiteral("App Default"),
        QVariantMap{{QStringLiteral("fps"), 90}});
    QVERIFY(!deviceProfile.isEmpty());
    QVERIFY(!libraryProfile.isEmpty());
    QVERIFY(!appProfile.isEmpty());

    QVERIFY(resolver->bindStreamingProfileScope(
        deviceProfile, QStringLiteral("client_device"), clientDeviceId));
    QVERIFY(resolver->bindStreamingProfileScope(
        libraryProfile, QStringLiteral("library_entry"), libraryEntryId));
    QVERIFY(resolver->bindStreamingProfileScope(
        appProfile, QStringLiteral("host_application"), hostApplicationKey));

    const QVariantMap context{
        {QStringLiteral("clientDeviceId"), clientDeviceId},
        {QStringLiteral("hostUuid"), hostUuid},
        {QStringLiteral("libraryEntryId"), libraryEntryId},
        {QStringLiteral("appId"), appId},
    };

    QVariantMap selection = resolver->resolve(context)
                                    .value(QStringLiteral("streamingProfile"))
                                    .toMap();
    QCOMPARE(selection.value(QStringLiteral("profileId")).toString(), appProfile);
    QCOMPARE(selection.value(QStringLiteral("scope")).toString(),
             QStringLiteral("host_application"));

    QVERIFY(resolver->unbindStreamingProfileScope(
        QStringLiteral("host_application"), hostApplicationKey));
    selection = resolver->resolve(context)
                        .value(QStringLiteral("streamingProfile")).toMap();
    QCOMPARE(selection.value(QStringLiteral("profileId")).toString(),
             libraryProfile);
    QCOMPARE(selection.value(QStringLiteral("scope")).toString(),
             QStringLiteral("library_entry"));

    QVERIFY(resolver->unbindStreamingProfileScope(
        QStringLiteral("library_entry"), libraryEntryId));
    selection = resolver->resolve(context)
                        .value(QStringLiteral("streamingProfile")).toMap();
    QCOMPARE(selection.value(QStringLiteral("profileId")).toString(),
             deviceProfile);
    QCOMPARE(selection.value(QStringLiteral("scope")).toString(),
             QStringLiteral("client_device"));
}

// A pinned scope binding suppresses a more-specific automatic switch and
// reports the suppressed candidate as a conflict instead of silently
// overriding the pin.
void TestEffectiveSettingsResolver::profilePinSuppressesAutomaticSwitchAndReportsConflict()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SettingsDatabase db;
    QVERIFY2(db.open(dir.filePath(QStringLiteral("jochona.db"))),
             qPrintable(db.lastError()));

    EffectiveSettingsResolver* resolver = EffectiveSettingsResolver::get();
    const QString clientDeviceId = QStringLiteral("pin-device");
    const QString hostUuid = QStringLiteral("pin-host");
    const QString appId = QStringLiteral("11");
    const QString hostApplicationKey =
        hostUuid + QStringLiteral("|") + appId;

    const QString pinnedProfile = resolver->saveStreamingProfile(
        QStringLiteral("Pinned"), QVariantMap{{QStringLiteral("fps"), 30}});
    const QString challengerProfile = resolver->saveStreamingProfile(
        QStringLiteral("Challenger"), QVariantMap{{QStringLiteral("fps"), 90}});
    QVERIFY(!pinnedProfile.isEmpty());
    QVERIFY(!challengerProfile.isEmpty());

    QVERIFY(resolver->bindStreamingProfileScope(
        pinnedProfile, QStringLiteral("client_device"), clientDeviceId,
        /*pinned=*/true));
    QVERIFY(resolver->bindStreamingProfileScope(
        challengerProfile, QStringLiteral("host_application"),
        hostApplicationKey));

    const QVariantMap selection = resolver->resolve(QVariantMap{
        {QStringLiteral("clientDeviceId"), clientDeviceId},
        {QStringLiteral("hostUuid"), hostUuid},
        {QStringLiteral("appId"), appId},
    }).value(QStringLiteral("streamingProfile")).toMap();

    QCOMPARE(selection.value(QStringLiteral("profileId")).toString(),
             pinnedProfile);
    QVERIFY(selection.value(QStringLiteral("pinned")).toBool());

    const QVariantList conflicts =
        selection.value(QStringLiteral("conflicts")).toList();
    QCOMPARE(conflicts.size(), 1);
    const QVariantMap conflict = conflicts.first().toMap();
    QCOMPARE(conflict.value(QStringLiteral("profileId")).toString(),
             challengerProfile);
    QCOMPARE(conflict.value(QStringLiteral("scope")).toString(),
             QStringLiteral("host_application"));
}

// Capability Safety remains the only layer allowed to change a pinned
// field's value, and doing so must be surfaced in pinConflicts rather than
// silently applied.
void TestEffectiveSettingsResolver::capabilitySafetyOverridesPinnedFieldAndReportsConflict()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SettingsDatabase db;
    QVERIFY2(db.open(dir.filePath(QStringLiteral("jochona.db"))),
             qPrintable(db.lastError()));

    const QString clientDeviceId = QStringLiteral("safety-device");
    QVERIFY(db.ensureClientDevice(clientDeviceId,
                                  QStringLiteral("Safety Test Device")));

    EffectiveSettingsResolver* resolver = EffectiveSettingsResolver::get();
    QVERIFY(resolver->setPatch(
        QStringLiteral("client_device"), clientDeviceId,
        QVariantMap{
            {QStringLiteral("width"), 99999},
            {QStringLiteral("height"), 99999},
        },
        QVariantMap{
            {QStringLiteral("width"), true},
            {QStringLiteral("height"), true},
        }));

    const QVariantMap resolved = resolver->resolve(QVariantMap{
        {QStringLiteral("clientDeviceId"), clientDeviceId},
    });
    const QVariantMap values = resolved.value(QStringLiteral("values")).toMap();
    // No display is attached in this headless test process, so Capability
    // Safety falls back to its 1920-wide default -- well under the pinned
    // 99999 request, proving the clamp actually ran.
    QVERIFY(values.value(QStringLiteral("width")).toInt() < 99999);

    const QVariantMap pinConflicts =
        resolved.value(QStringLiteral("pinConflicts")).toMap();
    QVERIFY(pinConflicts.contains(QStringLiteral("width")));
    QCOMPARE(pinConflicts.value(QStringLiteral("width")).toMap()
                 .value(QStringLiteral("requested")).toInt(), 99999);
    QCOMPARE(pinConflicts.value(QStringLiteral("width")).toMap()
                 .value(QStringLiteral("resolved")).toInt(),
             values.value(QStringLiteral("width")).toInt());
}
