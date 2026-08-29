#include "effectivesettingsresolver.h"

#include "core/settingsdatabase.h"
#include "settings/streamingpreferences.h"
#include "backend/negotiation/negotiator.h"

#include <QDateTime>
#include <QSysInfo>
#include <QUuid>

EffectiveSettingsResolver* EffectiveSettingsResolver::s_Instance = nullptr;

EffectiveSettingsResolver* EffectiveSettingsResolver::get()
{
    if (s_Instance == nullptr) {
        s_Instance = new EffectiveSettingsResolver();
    }
    return s_Instance;
}

EffectiveSettingsResolver::EffectiveSettingsResolver(QObject* parent)
    : QObject(parent)
{
    SettingsDatabase* database = SettingsDatabase::get();
    if (database != nullptr && database->isOpen()) {
        m_ClientDeviceId = database->setting(
                    QStringLiteral("client_device.id")).toString();
        if (m_ClientDeviceId.isEmpty()) {
            m_ClientDeviceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
            database->setSetting(QStringLiteral("client_device.id"),
                                 m_ClientDeviceId);
        }
        const QString defaultName = QSysInfo::machineHostName().isEmpty()
                ? tr("This device") : QSysInfo::machineHostName();
        const QString name = database->setting(
                    QStringLiteral("client_device.name"), defaultName).toString();
        database->ensureClientDevice(m_ClientDeviceId, name);
    }
}

QString EffectiveSettingsResolver::clientDeviceId() const
{
    return m_ClientDeviceId;
}

void EffectiveSettingsResolver::applyLayer(QVariantMap& values,
                                           QVariantMap& provenance,
                                           QVariantMap& pins,
                                           QVariantMap& floors,
                                           const QVariantMap& layer,
                                           const QVariantMap& layerPins,
                                           const QVariantMap& layerFloors,
                                           const QString& source,
                                           const QString& reason)
{
    for (auto it = layer.constBegin(); it != layer.constEnd(); ++it) {
        values.insert(it.key(), it.value());
        QVariantMap field = provenance.value(it.key()).toMap();
        field.insert(QStringLiteral("value"), it.value());
        field.insert(QStringLiteral("source"), source);
        field.insert(QStringLiteral("reason"), reason);
        field.insert(QStringLiteral("pinned"),
                     layerPins.value(it.key(), false).toBool());
        if (layerFloors.contains(it.key())) {
            field.insert(QStringLiteral("floor"), layerFloors.value(it.key()));
        }
        provenance.insert(it.key(), field);
    }
    for (auto it = layerPins.constBegin(); it != layerPins.constEnd(); ++it) {
        pins.insert(it.key(), it.value());
        if (provenance.contains(it.key())) {
            QVariantMap field = provenance.value(it.key()).toMap();
            field.insert(QStringLiteral("pinned"), it.value().toBool());
            provenance.insert(it.key(), field);
        }
    }
    for (auto it = layerFloors.constBegin(); it != layerFloors.constEnd(); ++it) {
        floors.insert(it.key(), it.value());
        if (provenance.contains(it.key())) {
            QVariantMap field = provenance.value(it.key()).toMap();
            field.insert(QStringLiteral("floor"), it.value());
            provenance.insert(it.key(), field);
        }
    }
}

QVariantMap EffectiveSettingsResolver::resolve(const QVariantMap& context) const
{
    StreamingPreferences* baseline = StreamingPreferences::get();
    QVariantMap values = baseline->toVariantMap();
    QVariantMap provenance;
    QVariantMap pins;
    QVariantMap floors;

    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        provenance.insert(it.key(), QVariantMap{
            {QStringLiteral("value"), it.value()},
            {QStringLiteral("source"), QStringLiteral("Settings Baseline")},
            {QStringLiteral("reason"), tr("Inherited from the Settings Baseline")},
            {QStringLiteral("pinned"), false},
        });
    }

    QString activeDisplayContextId;
    QVariantMap activeDisplayContext;
    SettingsDatabase* database = SettingsDatabase::get();
    if (database != nullptr && database->isOpen()) {
        const QString clientDeviceId = context.value(
                    QStringLiteral("clientDeviceId"),
                    m_ClientDeviceId).toString();
        const QVariantMap device =
            database->clientDeviceSettings(clientDeviceId);
        applyLayer(values, provenance, pins, floors,
                   device.value(QStringLiteral("values")).toMap(),
                   device.value(QStringLiteral("pins")).toMap(),
                   device.value(QStringLiteral("floors")).toMap(),
                   QStringLiteral("Client Device"),
                   tr("Settings for this Client Device"));

        activeDisplayContextId = context.value(
                    QStringLiteral("displayContextId"),
                    database->setting(
                        QStringLiteral("display.active_context_id")))
                .toString();
        activeDisplayContext =
            database->displayContext(activeDisplayContextId);
        applyLayer(values, provenance, pins, floors,
                   activeDisplayContext.value(
                       QStringLiteral("values")).toMap(),
                   activeDisplayContext.value(
                       QStringLiteral("pins")).toMap(),
                   activeDisplayContext.value(
                       QStringLiteral("floors")).toMap(),
                   QStringLiteral("Display Context"),
                   tr("Settings for the active display and dock topology"));


        auto applyPatch = [&](const QString& scope,
                              const QString& key,
                              const QString& source) {
            if (key.isEmpty()) {
                return;
            }
            const QVariantMap bundle = database->settingsPatch(scope, key);
            if (bundle.isEmpty()) {
                return;
            }
            applyLayer(values, provenance, pins, floors,
                       bundle.value(QStringLiteral("values")).toMap(),
                       bundle.value(QStringLiteral("pins")).toMap(),
                       bundle.value(QStringLiteral("floors")).toMap(),
                       source, tr("Explicit sparse Settings Patch"));
        };

        const QString hostUuid = context.value(QStringLiteral("hostUuid")).toString();
        const QString deviceId = context.value(
                    QStringLiteral("clientDeviceId"), m_ClientDeviceId).toString();
        const QString pairKey = context.value(QStringLiteral("pairKey"),
                                               deviceId + "|" + hostUuid).toString();
        applyPatch(QStringLiteral("host_client_pair"), pairKey,
                   QStringLiteral("Host–Client Pair"));
        applyPatch(QStringLiteral("library_entry"),
                   context.value(QStringLiteral("libraryEntryId")).toString(),
                   QStringLiteral("Library Entry"));

        QString hostApplicationKey = context.value(
                    QStringLiteral("hostApplicationKey")).toString();
        if (hostApplicationKey.isEmpty() && !hostUuid.isEmpty()
                && context.contains(QStringLiteral("appId"))) {
            hostApplicationKey = hostUuid + "|"
                    + context.value(QStringLiteral("appId")).toString();
        }
        applyPatch(QStringLiteral("host_application"), hostApplicationKey,
                   QStringLiteral("Host Application"));
        QString profileId =
            context.value(QStringLiteral("profileId")).toString();
        if (profileId.isEmpty()) {
            profileId = activeDisplayContext.value(
                        QStringLiteral("profileId")).toString();
        }
        const QVariantMap profile = database->streamingProfile(profileId);
        if (!profile.isEmpty()) {
            applyLayer(values, provenance, pins, floors,
                       profile.value(QStringLiteral("values")).toMap(), {}, {},
                       QStringLiteral("Streaming Profile"),
                       tr("Selected Streaming Profile: %1")
                       .arg(profile.value(QStringLiteral("name")).toString()));
        }
    }

    applyLayer(values, provenance, pins, floors,
               context.value(QStringLiteral("sessionPatch")).toMap(),
               context.value(QStringLiteral("sessionPins")).toMap(),
               context.value(QStringLiteral("sessionFloors")).toMap(),
               QStringLiteral("Session Patch"),
               tr("Changed for this Session"));

    // Final capability safety is the only layer allowed to lower pinned
    // values. Keep canonical quality aliases alongside legacy preference keys
    // until StreamingPreferences' serialized names are retired.
    QString codec = QStringLiteral("auto");
    switch (values.value(QStringLiteral("videocfg")).toInt()) {
    case StreamingPreferences::VCC_FORCE_H264:
        codec = QStringLiteral("h264");
        break;
    case StreamingPreferences::VCC_FORCE_HEVC:
    case StreamingPreferences::VCC_FORCE_HEVC_HDR_DEPRECATED:
        codec = QStringLiteral("hevc");
        break;
    case StreamingPreferences::VCC_FORCE_AV1:
        codec = QStringLiteral("av1");
        break;
    default:
        break;
    }
    if (values.contains(QStringLiteral("codec"))) {
        codec = values.value(QStringLiteral("codec")).toString();
    }
    const int bitrate = values.value(
                QStringLiteral("bitrateKbps"),
                values.value(QStringLiteral("bitrate"))).toInt();
    const QVariantMap candidate{
        {QStringLiteral("width"), values.value(QStringLiteral("width"))},
        {QStringLiteral("height"), values.value(QStringLiteral("height"))},
        {QStringLiteral("fps"), values.value(QStringLiteral("fps"))},
        {QStringLiteral("bitrateKbps"), bitrate},
        {QStringLiteral("codec"), codec},
    };
    const QString hostUuid = context.value(QStringLiteral("hostUuid")).toString();
    const QVariantMap safe = Negotiator::get()->clampQualityFor(hostUuid, candidate);
    const QVariantMap safetyReasons = safe.value(QStringLiteral("reasons")).toMap();
    QVariantMap floorConflicts;

    auto applySafety = [&](const QString& key, const QVariant& safeValue,
                           const QString& legacyKey) {
        const QVariant requested = candidate.value(key);
        if (requested != safeValue) {
            QVariantMap field = provenance.value(key).toMap();
            if (field.isEmpty() && !legacyKey.isEmpty()) {
                field = provenance.value(legacyKey).toMap();
            }
            field.insert(QStringLiteral("value"), safeValue);
            field.insert(QStringLiteral("source"),
                         QStringLiteral("Capability Safety"));
            field.insert(QStringLiteral("reason"), safetyReasons.value(key));
            field.insert(QStringLiteral("pinned"),
                         pins.value(key, false).toBool());
            provenance.insert(key, field);
        }
        values.insert(key, safeValue);
        if (!legacyKey.isEmpty()) {
            values.insert(legacyKey, safeValue);
        }
        if (floors.contains(key)
                && safeValue.canConvert<double>()
                && safeValue.toDouble() < floors.value(key).toDouble()) {
            floorConflicts.insert(key, QVariantMap{
                {QStringLiteral("floor"), floors.value(key)},
                {QStringLiteral("resolved"), safeValue},
                {QStringLiteral("reason"), safetyReasons.value(key)},
            });
        }
    };

    applySafety(QStringLiteral("width"), safe.value(QStringLiteral("width")),
                QString());
    applySafety(QStringLiteral("height"), safe.value(QStringLiteral("height")),
                QString());
    applySafety(QStringLiteral("fps"), safe.value(QStringLiteral("fps")),
                QString());
    applySafety(QStringLiteral("bitrateKbps"),
                safe.value(QStringLiteral("bitrateKbps")),
                QStringLiteral("bitrate"));
    applySafety(QStringLiteral("codec"), safe.value(QStringLiteral("codec")),
                QStringLiteral("videocfg"));
    values.insert(QStringLiteral("bitrate"),
                  safe.value(QStringLiteral("bitrateKbps")));
    const QString safeCodec = safe.value(QStringLiteral("codec")).toString();
    values.insert(QStringLiteral("videocfg"),
                  safeCodec == QStringLiteral("h264")
                  ? StreamingPreferences::VCC_FORCE_H264
                  : safeCodec == QStringLiteral("hevc")
                    ? StreamingPreferences::VCC_FORCE_HEVC
                    : safeCodec == QStringLiteral("av1")
                      ? StreamingPreferences::VCC_FORCE_AV1
                      : StreamingPreferences::VCC_AUTO);


    if (!activeDisplayContext.isEmpty()
            && !activeDisplayContext.value(
                    QStringLiteral("hdrCapable")).toBool()
            && values.value(QStringLiteral("hdr")).toBool()) {
        values.insert(QStringLiteral("hdr"), false);
        provenance.insert(QStringLiteral("hdr"), QVariantMap{
            {QStringLiteral("value"), false},
            {QStringLiteral("source"), QStringLiteral("Capability Safety")},
            {QStringLiteral("reason"),
             tr("The active display does not report HDR output capability")},
            {QStringLiteral("pinned"), false},
        });
    }

    QVariantMap resolvedContext = context;
    resolvedContext.insert(QStringLiteral("clientDeviceId"),
                           m_ClientDeviceId);
    resolvedContext.insert(QStringLiteral("displayContextId"),
                           activeDisplayContextId);
    return {
        {QStringLiteral("values"), values},
        {QStringLiteral("provenance"), provenance},
        {QStringLiteral("pins"), pins},
        {QStringLiteral("floors"), floors},
        {QStringLiteral("floorConflicts"), floorConflicts},
        {QStringLiteral("context"), resolvedContext},
    };
}

bool EffectiveSettingsResolver::setPatch(const QString& scope,
                                         const QString& contextKey,
                                         const QVariantMap& values,
                                         const QVariantMap& pins,
                                         const QVariantMap& floors)
{
    SettingsDatabase* database = SettingsDatabase::get();
    if (database == nullptr || !database->isOpen()) {
        return false;
    }
    bool saved = false;
    if (scope == QStringLiteral("client_device")) {
        saved = database->setClientDeviceSettings(
            contextKey.isEmpty() ? m_ClientDeviceId : contextKey,
            values, pins, floors);
    } else if (scope == QStringLiteral("display_context")) {
        const QVariantMap current =
            database->displayContext(contextKey);
        saved = database->setDisplayContextSettings(
            contextKey, values, pins, floors,
            current.value(QStringLiteral("profileId")).toString());
    } else {
        saved = database->setSettingsPatch(scope, contextKey,
                                           values, pins, floors);
    }
    if (saved) {
        QVariantMap context;
        context.insert(QStringLiteral("scope"), scope);
        context.insert(QStringLiteral("contextKey"), contextKey);
        emit resolvedChanged(context);
    }
    return saved;
}

QVariantMap EffectiveSettingsResolver::patch(const QString& scope,
                                             const QString& contextKey) const
{
    SettingsDatabase* database = SettingsDatabase::get();
    if (database == nullptr || !database->isOpen()) return {};
    if (scope == QStringLiteral("client_device")) {
        return database->clientDeviceSettings(
            contextKey.isEmpty() ? m_ClientDeviceId : contextKey);
    }
    if (scope == QStringLiteral("display_context")) {
        return database->displayContext(contextKey);
    }
    return database->settingsPatch(scope, contextKey);
}

StreamingPreferences* EffectiveSettingsResolver::createPreferences(
        const QVariantMap& context) const
{
    StreamingPreferences* preferences = StreamingPreferences::get()->clone();
    preferences->applyVariantMap(resolve(context)
                                 .value(QStringLiteral("values")).toMap());
    return preferences;
}

QVariantList EffectiveSettingsResolver::streamingProfiles() const
{
    SettingsDatabase* database = SettingsDatabase::get();
    return database != nullptr && database->isOpen()
        ? database->streamingProfiles() : QVariantList();
}

QString EffectiveSettingsResolver::saveStreamingProfile(
        const QString& name,
        const QVariantMap& values,
        const QString& profileId)
{
    SettingsDatabase* database = SettingsDatabase::get();
    if (database == nullptr || !database->isOpen()) return {};
    const QString id = profileId.isEmpty()
        ? QUuid::createUuid().toString(QUuid::WithoutBraces)
        : profileId;
    if (!database->saveStreamingProfile(id, name, values)) return {};
    emit streamingProfilesChanged();
    return id;
}

bool EffectiveSettingsResolver::deleteStreamingProfile(
        const QString& profileId)
{
    SettingsDatabase* database = SettingsDatabase::get();
    if (database == nullptr
            || !database->deleteStreamingProfile(profileId)) {
        return false;
    }
    emit streamingProfilesChanged();
    return true;
}

bool EffectiveSettingsResolver::setDisplayStreamingProfile(
        const QString& displayContextId,
        const QString& profileId)
{
    SettingsDatabase* database = SettingsDatabase::get();
    if (database == nullptr || !database->isOpen()
            || displayContextId.isEmpty()) {
        return false;
    }
    const QVariantMap current =
        database->displayContext(displayContextId);
    if (current.isEmpty()) return false;
    const bool saved = database->setDisplayContextSettings(
        displayContextId,
        current.value(QStringLiteral("values")).toMap(),
        current.value(QStringLiteral("pins")).toMap(),
        current.value(QStringLiteral("floors")).toMap(),
        profileId);
    if (saved) {
        emit resolvedChanged({
            {QStringLiteral("scope"), QStringLiteral("display_context")},
            {QStringLiteral("contextKey"), displayContextId},
        });
    }
    return saved;
}
