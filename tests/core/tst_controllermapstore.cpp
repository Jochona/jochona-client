#include "tst_controllermapstore.h"

#include "backend/controllerprofilestore.h"

#include <Limelight.h>
#include <QMap>
#include <QSignalSpy>
#include <QTest>

namespace {
class MemoryControllerMapBackend : public IControllerMapBackend
{
public:
    QVariantMap load(const QString& controllerId,
                     const QString& contextKey) const override
    {
        return values.value(controllerId + QLatin1Char('|') + contextKey);
    }

    void save(const QString& controllerId,
              const QString& contextKey,
              const QVariantMap& config) override
    {
        values.insert(controllerId + QLatin1Char('|') + contextKey, config);
    }

    void remove(const QString& controllerId,
                const QString& contextKey) override
    {
        values.remove(controllerId + QLatin1Char('|') + contextKey);
    }

    QStringList knownControllerPaths() const override
    {
        QStringList ids;
        for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
            const QString id = it.key().section(QLatin1Char('|'), 0, 0);
            if (!ids.contains(id)) ids.append(id);
        }
        ids.sort();
        return ids;
    }

    mutable QMap<QString, QVariantMap> values;
};
}

void TestControllerMapStore::layersSparseMapsInDeclaredOrder()
{
    auto* backend = new MemoryControllerMapBackend();
    backend->save(QStringLiteral("pad"), QString(), {
        {QStringLiteral("calibration"), QVariantMap{
             {QStringLiteral("deadzoneLeftStick"), 0.20}}},
        {QStringLiteral("buttonRemap"), QVariantMap{
             {QStringLiteral("a"), QStringLiteral("b")}}},
    });
    backend->save(QStringLiteral("pad"),
                  QStringLiteral("library_entry:entry"), {
        {QStringLiteral("calibration"), QVariantMap{
             {QStringLiteral("deadzoneRightStick"), 0.30}}},
        {QStringLiteral("buttonRemap"), QVariantMap{
             {QStringLiteral("x"), QStringLiteral("y")}}},
    });
    backend->save(QStringLiteral("pad"),
                  QStringLiteral("host_application:host|7"), {
        {QStringLiteral("calibration"), QVariantMap{
             {QStringLiteral("deadzoneLeftStick"), 0.05}}},
        {QStringLiteral("buttonRemap"), QVariantMap{
             {QStringLiteral("a"), QStringLiteral("x")}}},
    });

    ControllerMapStore store(backend);
    const QVariantMap effective = store.mapFor(
        QStringLiteral("pad"), QStringLiteral("entry"),
        QStringLiteral("host|7"));
    const QVariantMap calibration =
        effective.value(QStringLiteral("calibration")).toMap();
    QCOMPARE(calibration.value(
                 QStringLiteral("deadzoneLeftStick")).toDouble(), 0.05);
    QCOMPARE(calibration.value(
                 QStringLiteral("deadzoneRightStick")).toDouble(), 0.30);
    QCOMPARE(calibration.value(
                 QStringLiteral("curveLeftStick")).toDouble(), 1.0);
    const QVariantMap remap =
        effective.value(QStringLiteral("buttonRemap")).toMap();
    QCOMPARE(remap.value(QStringLiteral("a")).toString(),
             QStringLiteral("x"));
    QCOMPARE(remap.value(QStringLiteral("x")).toString(),
             QStringLiteral("y"));
}

void TestControllerMapStore::saveAndResetUseExplicitScopes()
{
    auto* backend = new MemoryControllerMapBackend();
    ControllerMapStore store(backend);
    QSignalSpy changed(&store, &ControllerMapStore::mapChanged);

    const QVariantMap patch{
        {QStringLiteral("buttonRemap"), QVariantMap{
             {QStringLiteral("a"), QStringLiteral("b")}}},
    };
    store.saveMap(QStringLiteral("pad"),
                  QStringLiteral("host_application"),
                  QStringLiteral("host|9"), patch);
    QCOMPARE(backend->load(QStringLiteral("pad"),
                           QStringLiteral("host_application:host|9")),
             patch);
    QCOMPARE(changed.count(), 1);

    store.resetMap(QStringLiteral("pad"),
                   QStringLiteral("host_application"),
                   QStringLiteral("host|9"));
    QVERIFY(backend->load(QStringLiteral("pad"),
                          QStringLiteral("host_application:host|9"))
                .isEmpty());
    QCOMPARE(changed.count(), 2);
}

void TestControllerMapStore::roundTripsControllerTransmissionMode()
{
    ControllerMap map;
    map.controllerPath = QStringLiteral("pad");
    map.rawPassthrough = true;
    map.transmissionMode = ControllerTransmissionMode::Compatible;

    const QVariantMap serialized = map.toVariantMap();
    QCOMPARE(serialized.value(QStringLiteral("rawPassthrough")).toBool(), true);
    QCOMPARE(serialized.value(QStringLiteral("transmissionMode")).toString(),
             QStringLiteral("compatible"));

    const ControllerMap roundTripped = ControllerMap::fromVariantMap(
        QStringLiteral("pad"), QString(), serialized);
    QCOMPARE(roundTripped.rawPassthrough, true);
    QVERIFY(roundTripped.transmissionMode
            == ControllerTransmissionMode::Compatible);

    // A map saved before this feature existed has neither field; it must
    // decode to the prior, always-on behavior (Native, no passthrough).
    const ControllerMap legacyDefaults =
        ControllerMap::fromVariantMap(QStringLiteral("pad"), QString(), {});
    QCOMPARE(legacyDefaults.rawPassthrough, false);
    QVERIFY(legacyDefaults.transmissionMode
            == ControllerTransmissionMode::Native);
}

void TestControllerMapStore::preservesRawPassthroughAndTransmissionModeAcrossLayers()
{
    auto* backend = new MemoryControllerMapBackend();
    backend->save(QStringLiteral("pad"), QString(), {
        {QStringLiteral("rawPassthrough"), false},
        {QStringLiteral("transmissionMode"), QStringLiteral("native")},
    });

    ControllerMapStore store(backend);

    const QVariantMap controllerWide = store.mapFor(QStringLiteral("pad"));
    QCOMPARE(controllerWide.value(QStringLiteral("rawPassthrough")).toBool(),
             false);
    QCOMPARE(controllerWide.value(QStringLiteral("transmissionMode")).toString(),
             QStringLiteral("native"));

    // A per-game (Library Entry) override switches only that game to Raw
    // Passthrough + Compatible transmission -- the controller-wide map
    // (used by every other game) is untouched.
    store.saveMap(QStringLiteral("pad"), QStringLiteral("library_entry"),
                  QStringLiteral("entry"), {
        {QStringLiteral("calibration"), QVariantMap()},
        {QStringLiteral("buttonRemap"), QVariantMap()},
        {QStringLiteral("rawPassthrough"), true},
        {QStringLiteral("transmissionMode"), QStringLiteral("compatible")},
    });

    const QVariantMap gameEffective =
        store.mapFor(QStringLiteral("pad"), QStringLiteral("entry"));
    QCOMPARE(gameEffective.value(QStringLiteral("rawPassthrough")).toBool(),
             true);
    QCOMPARE(gameEffective.value(QStringLiteral("transmissionMode")).toString(),
             QStringLiteral("compatible"));

    const QVariantMap stillControllerWide = store.mapFor(QStringLiteral("pad"));
    QCOMPARE(stillControllerWide.value(
                 QStringLiteral("rawPassthrough")).toBool(), false);
    QCOMPARE(stillControllerWide.value(
                 QStringLiteral("transmissionMode")).toString(),
             QStringLiteral("native"));
}

void TestControllerMapStore::compatibleTransmissionMasksToGenericPad()
{
    uint8_t type = LI_CTYPE_PS;
    uint32_t capabilities = LI_CCAP_ANALOG_TRIGGERS | LI_CCAP_RUMBLE
        | LI_CCAP_TOUCHPAD | LI_CCAP_ACCEL | LI_CCAP_GYRO | LI_CCAP_RGB_LED;
    uint32_t buttonFlags =
        A_FLAG | B_FLAG | PADDLE1_FLAG | TOUCHPAD_FLAG | MISC_FLAG;

    ControllerMap::applyCompatibleTransmission(
        ControllerTransmissionMode::Compatible, type, capabilities,
        buttonFlags);

    QCOMPARE(type, static_cast<uint8_t>(LI_CTYPE_XBOX));
    // Widely-supported capabilities survive Compatible masking...
    QVERIFY((capabilities & LI_CCAP_ANALOG_TRIGGERS) != 0);
    QVERIFY((capabilities & LI_CCAP_RUMBLE) != 0);
    // ...exotic/device-specific ones do not.
    QVERIFY((capabilities & LI_CCAP_TOUCHPAD) == 0);
    QVERIFY((capabilities & LI_CCAP_ACCEL) == 0);
    QVERIFY((capabilities & LI_CCAP_GYRO) == 0);
    QVERIFY((capabilities & LI_CCAP_RGB_LED) == 0);
    // Standard face buttons survive; paddles/touchpad/share do not.
    QVERIFY((buttonFlags & A_FLAG) != 0);
    QVERIFY((buttonFlags & B_FLAG) != 0);
    QVERIFY((buttonFlags & PADDLE1_FLAG) == 0);
    QVERIFY((buttonFlags & TOUCHPAD_FLAG) == 0);
    QVERIFY((buttonFlags & MISC_FLAG) == 0);
}

void TestControllerMapStore::nativeTransmissionLeavesReportedProfileUnchanged()
{
    uint8_t type = LI_CTYPE_PS;
    uint32_t capabilities = LI_CCAP_TOUCHPAD | LI_CCAP_GYRO;
    uint32_t buttonFlags = PADDLE1_FLAG | TOUCHPAD_FLAG;

    ControllerMap::applyCompatibleTransmission(
        ControllerTransmissionMode::Native, type, capabilities,
        buttonFlags);

    QCOMPARE(type, static_cast<uint8_t>(LI_CTYPE_PS));
    QCOMPARE(capabilities,
             static_cast<uint32_t>(LI_CCAP_TOUCHPAD | LI_CCAP_GYRO));
    QCOMPARE(buttonFlags,
             static_cast<uint32_t>(PADDLE1_FLAG | TOUCHPAD_FLAG));
}
