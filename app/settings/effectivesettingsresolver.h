#pragma once

#include <QObject>
#include <QVariantMap>
#include <QVariantList>

class StreamingPreferences;

// Resolves one Session's settings behind one context-shaped interface. Every
// field includes provenance so QML and Support Bundles can explain the result.
class EffectiveSettingsResolver : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString clientDeviceId READ clientDeviceId CONSTANT)
    Q_PROPERTY(QVariantList streamingProfiles READ streamingProfiles
               NOTIFY streamingProfilesChanged)

public:
    static EffectiveSettingsResolver* get();

    Q_INVOKABLE QVariantMap resolve(const QVariantMap& context) const;
    Q_INVOKABLE bool setPatch(const QString& scope,
                              const QString& contextKey,
                              const QVariantMap& values,
                              const QVariantMap& pins = {},
                              const QVariantMap& floors = {});
    Q_INVOKABLE QVariantMap patch(const QString& scope,
                                  const QString& contextKey) const;

    StreamingPreferences* createPreferences(const QVariantMap& context) const;
    QString clientDeviceId() const;
    QVariantList streamingProfiles() const;
    Q_INVOKABLE QString saveStreamingProfile(
            const QString& name,
            const QVariantMap& values,
            const QString& profileId = QString());
    Q_INVOKABLE bool deleteStreamingProfile(const QString& profileId);
    Q_INVOKABLE bool setDisplayStreamingProfile(
            const QString& displayContextId,
            const QString& profileId);

signals:
    void resolvedChanged(const QVariantMap& context);
    void streamingProfilesChanged();

private:
    explicit EffectiveSettingsResolver(QObject* parent = nullptr);

    static void applyLayer(QVariantMap& values,
                           QVariantMap& provenance,
                           QVariantMap& pins,
                           QVariantMap& floors,
                           const QVariantMap& layer,
                           const QVariantMap& layerPins,
                           const QVariantMap& layerFloors,
                           const QString& source,
                           const QString& reason);

    static EffectiveSettingsResolver* s_Instance;
    QString m_ClientDeviceId;
};
