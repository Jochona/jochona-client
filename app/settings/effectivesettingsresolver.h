#pragma once

#include <QObject>
#include <QVariantMap>
#include <QVariantList>
#include <functional>
class StreamingPreferences;
class SettingsDatabase;

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
    using BaselineProvider = std::function<QVariantMap()>;
    using CapabilitySafetyProvider =
        std::function<QVariantMap(const QString&, const QVariantMap&)>;

    static void setBaselineProvider(BaselineProvider provider);
    static void setCapabilitySafetyProvider(
        CapabilitySafetyProvider provider);

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

    // Streaming Profile scope bindings let a saved Profile be selected
    // automatically for a Client Device, Host, Library Entry, or Host
    // Application (Display Context binding is the existing dedicated
    // display_context/profileId pin -- routed here too for one API).
    // `pinned` protects the binding from being superseded by a
    // higher-specificity automatic match; a suppressed switch is reported
    // in resolve()'s streamingProfile.conflicts, not applied silently.
    Q_INVOKABLE bool bindStreamingProfileScope(const QString& profileId,
                                               const QString& scope,
                                               const QString& contextKey,
                                               bool pinned = false);
    Q_INVOKABLE bool unbindStreamingProfileScope(const QString& scope,
                                                 const QString& contextKey);
    Q_INVOKABLE QVariantList streamingProfileScopeBindings(
            const QString& profileId) const;

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

    // Deterministic most-specific Streaming Profile selection across the
    // Client Device, Display Context, Host, Library Entry, and Host
    // Application scopes (ascending specificity). An explicit
    // context["profilePinId"] always wins. Returns {} (empty "profileId")
    // when nothing matches. See effectivesettingsresolver.cpp for the full
    // tie-break and pin-suppression contract.
    QVariantMap selectStreamingProfile(SettingsDatabase* database,
                                       const QVariantMap& context,
                                       const QString& clientDeviceId,
                                       const QString& displayContextId,
                                       const QVariantMap& activeDisplayContext,
                                       const QString& hostUuid,
                                       const QString& libraryEntryId,
                                       const QString& hostApplicationKey) const;

    static EffectiveSettingsResolver* s_Instance;
    static BaselineProvider s_BaselineProvider;
    static CapabilitySafetyProvider s_CapabilitySafetyProvider;
    QString m_ClientDeviceId;
};
