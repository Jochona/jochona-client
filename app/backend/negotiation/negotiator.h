// Jochona: per-device quality negotiation (proposal: "settings that fit the
// device it is running on, the host it connects to, and the game being
// launched" + "Auto" per-host override). Pure decision layer: given what this
// machine can display/decode, what the host advertised, the user's global
// preferences, and any per-host override, compute the effective stream quality
// plus a human-readable reason for every field. It deliberately does NOT talk
// to Session yet — the streaming wiring lands as a separate integration step.
#pragma once

#include <QObject>
#include <QVariantMap>
#include <QList>

class SystemProperties;
class ComputerManager;
class Negotiator : public QObject
{
    Q_OBJECT

public:
    Q_INVOKABLE static Negotiator* get();

    // These QML singletons have no C++ accessor; main.cpp's registration
    // providers hand the live instances over. Null until QML first creates
    // them — every use site guards.
    static void setSystemProperties(SystemProperties* properties);
    static void setComputerManager(ComputerManager* manager);

    // This machine's presentation/decode capabilities, as observed live:
    //   name, width, height, maxRefresh, hdr, maxStreamWidth, maxStreamHeight,
    //   hardwareDecoder
    // hdr is best-effort: Qt only reports float encoding support on
    // recent versions; false does not guarantee "no HDR".
    Q_INVOKABLE QVariantMap deviceProfile() const;

    // All connected screens; first entry equals deviceProfile() (primary).
    Q_INVOKABLE QList<QVariantMap> displays() const;

    // Legacy QML convenience: Settings Baseline + Pair patch, then safety.
    Q_INVOKABLE QVariantMap effectiveQualityFor(const QString& uuid) const;

    // Capability safety stage for Effective Settings. `candidate` keys:
    // width, height, fps, bitrateKbps, codec. Returns adjusted values plus
    // `reasons` and `auto` (true when a safety/automatic adjustment occurred).
    Q_INVOKABLE QVariantMap clampQualityFor(const QString& uuid,
                                            const QVariantMap& candidate) const;

    // Compatibility Pair patch until contextual editors move to
    // EffectiveSettings. Values are sparse.
    Q_INVOKABLE void setQualityOverride(const QString& uuid, const QVariantMap& override_);
    Q_INVOKABLE void clearQualityOverride(const QString& uuid);
    Q_INVOKABLE QVariantMap qualityOverride(const QString& uuid) const;

signals:
    void deviceProfileChanged();
    void qualityOverridesChanged();

private slots:
    void handleScreenCountChanged();

private:
    Negotiator();

    QVariantMap loadOverridesFromDatabase(const QString& uuid) const;
    void saveOverridesToDatabase(const QString& uuid, const QVariantMap& value);

    static SystemProperties* s_Properties;
    static ComputerManager* s_Manager;
};
