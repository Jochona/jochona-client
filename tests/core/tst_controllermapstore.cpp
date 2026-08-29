#include "tst_controllermapstore.h"

#include "backend/controllerprofilestore.h"

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
