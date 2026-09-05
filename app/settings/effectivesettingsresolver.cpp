#include "effectivesettingsresolver.h"

#include "core/settingsdatabase.h"
#include "settings/streamingpreferences.h"

#include <QDateTime>
#include <QMap>
#include <QSysInfo>
#include <QUuid>

#include <utility>

namespace {
// Reserved key inside a Streaming Profile's stored values that carries its
// scope bindings (Client Device / Host / Library Entry / Host Application).
// Never merged into resolved settings values -- always stripped before a
// profile's values are applied as a layer or handed to callers.
const QString kScopeBindingsKey = QStringLiteral("__scopeBindings");

// Display Context binding stays on its own dedicated
// display_contexts.streaming_profile_id column (set via
// EffectiveSettingsResolver::setDisplayStreamingProfile); it is not
// duplicated into __scopeBindings.
const QStringList kBindableScopes = {
    QStringLiteral("client_device"),
    QStringLiteral("display_context"),
    QStringLiteral("host"),
    QStringLiteral("library_entry"),
    QStringLiteral("host_application"),
};

// Ascending specificity: later scopes override earlier ones when both have
// a matching automatic binding for the current context.
const QStringList kScopeSpecificityOrder = {
    QStringLiteral("client_device"),
    QStringLiteral("display_context"),
    QStringLiteral("host"),
    QStringLiteral("library_entry"),
    QStringLiteral("host_application"),
};

QString scopeLabel(const QString& scope)
{
    if (scope == QStringLiteral("client_device")) return QStringLiteral("Client Device");
    if (scope == QStringLiteral("display_context")) return QStringLiteral("Display Context");
    if (scope == QStringLiteral("host")) return QStringLiteral("Host");
    if (scope == QStringLiteral("library_entry")) return QStringLiteral("Library Entry");
    if (scope == QStringLiteral("host_application")) return QStringLiteral("Host Application");
    if (scope == QStringLiteral("session_pin")) return QStringLiteral("Session Profile Pin");
    return scope;
}

struct ProfileCandidate
{
    QString profileId;
    QString name;
    bool pinned = false;
    QString updatedAt;
};
}

EffectiveSettingsResolver* EffectiveSettingsResolver::s_Instance = nullptr;
EffectiveSettingsResolver::BaselineProvider
    EffectiveSettingsResolver::s_BaselineProvider;
EffectiveSettingsResolver::CapabilitySafetyProvider
    EffectiveSettingsResolver::s_CapabilitySafetyProvider;

void EffectiveSettingsResolver::setBaselineProvider(
        BaselineProvider provider)
{
    s_BaselineProvider = std::move(provider);
}

void EffectiveSettingsResolver::setCapabilitySafetyProvider(
        CapabilitySafetyProvider provider)
{
    s_CapabilitySafetyProvider = std::move(provider);
}

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

QVariantMap EffectiveSettingsResolver::selectStreamingProfile(
        SettingsDatabase* database,
        const QVariantMap& context,
        const QString& clientDeviceId,
        const QString& displayContextId,
        const QVariantMap& activeDisplayContext,
        const QString& hostUuid,
        const QString& libraryEntryId,
        const QString& hostApplicationKey) const
{
    QVariantMap result;

    // An explicit Session Profile Pin always wins outright: it is the most
    // specific possible override (the same precedence tier as a Session
    // Patch) and locks every field the profile supplies.
    const QString explicitPinId =
        context.value(QStringLiteral("profilePinId")).toString();
    if (!explicitPinId.isEmpty()) {
        const QVariantMap profile = database->streamingProfile(explicitPinId);
        if (!profile.isEmpty()) {
            result.insert(QStringLiteral("profileId"), explicitPinId);
            result.insert(QStringLiteral("name"), profile.value(QStringLiteral("name")));
            result.insert(QStringLiteral("scope"), QStringLiteral("session_pin"));
            result.insert(QStringLiteral("contextKey"), QString());
            result.insert(QStringLiteral("pinned"), true);
            result.insert(QStringLiteral("reason"),
                         tr("Pinned for this Session: %1")
                         .arg(profile.value(QStringLiteral("name")).toString()));
            result.insert(QStringLiteral("conflicts"), QVariantList());
            return result;
        }
        // The pin references a Profile that no longer exists; fall through
        // to automatic selection rather than silently dropping the request.
    }

    QVariantMap contextKeys;
    contextKeys.insert(QStringLiteral("client_device"), clientDeviceId);
    contextKeys.insert(QStringLiteral("display_context"), displayContextId);
    contextKeys.insert(QStringLiteral("host"), hostUuid);
    contextKeys.insert(QStringLiteral("library_entry"), libraryEntryId);
    contextKeys.insert(QStringLiteral("host_application"), hostApplicationKey);

    QMap<QString, ProfileCandidate> candidatesByScope;

    // Display Context is authoritative via its own dedicated column/pin
    // (setDisplayStreamingProfile); its documented behavior ("a display can
    // pin one Profile") makes it pinned by default.
    const QString displayProfileId =
        activeDisplayContext.value(QStringLiteral("profileId")).toString();
    if (!displayProfileId.isEmpty() && !displayContextId.isEmpty()) {
        const QVariantMap profile = database->streamingProfile(displayProfileId);
        if (!profile.isEmpty()) {
            candidatesByScope.insert(QStringLiteral("display_context"),
                ProfileCandidate{displayProfileId,
                                 profile.value(QStringLiteral("name")).toString(),
                                 true, QString()});
        }
    }

    const QVariantList profiles = database->streamingProfiles();
    for (const QVariant& entry : profiles) {
        const QVariantMap profile = entry.toMap();
        const QString profileId = profile.value(QStringLiteral("id")).toString();
        const QString name = profile.value(QStringLiteral("name")).toString();
        const QString updatedAt = profile.value(QStringLiteral("updatedAt")).toString();
        const QVariantList bindings =
            profile.value(QStringLiteral("values")).toMap()
                   .value(kScopeBindingsKey).toList();
        for (const QVariant& bindingEntry : bindings) {
            const QVariantMap binding = bindingEntry.toMap();
            const QString scope = binding.value(QStringLiteral("scope")).toString();
            if (scope == QStringLiteral("display_context")) {
                continue; // authoritative source is the dedicated column above
            }
            const QString key = binding.value(QStringLiteral("key")).toString();
            if (key.isEmpty() || contextKeys.value(scope).toString() != key) {
                continue;
            }
            const bool pinned = binding.value(QStringLiteral("pinned"), false).toBool();
            const ProfileCandidate incoming{profileId, name, pinned, updatedAt};
            auto existing = candidatesByScope.constFind(scope);
            if (existing == candidatesByScope.constEnd()
                    || incoming.updatedAt > existing->updatedAt
                    || (incoming.updatedAt == existing->updatedAt
                        && incoming.profileId < existing->profileId)) {
                candidatesByScope.insert(scope, incoming);
            }
        }
    }

    QVariantList conflicts;
    QString selectedScope;
    ProfileCandidate selected;
    for (const QString& scope : kScopeSpecificityOrder) {
        const QString key = contextKeys.value(scope).toString();
        if (key.isEmpty()) continue;
        const auto it = candidatesByScope.constFind(scope);
        if (it == candidatesByScope.constEnd()) continue;
        const ProfileCandidate& candidate = it.value();

        if (selected.profileId.isEmpty()) {
            selected = candidate;
            selectedScope = scope;
        } else if (candidate.profileId == selected.profileId) {
            selectedScope = scope; // more specific scope agrees; sharpen provenance
        } else if (selected.pinned) {
            // A less-specific pinned binding suppresses this automatic
            // switch; surface it instead of silently overriding the pin.
            conflicts.append(QVariantMap{
                {QStringLiteral("scope"), scope},
                {QStringLiteral("profileId"), candidate.profileId},
                {QStringLiteral("name"), candidate.name},
                {QStringLiteral("supersededBy"), selectedScope},
                {QStringLiteral("reason"),
                 tr("%1 would select \"%2\", but \"%3\" is pinned from %4")
                 .arg(scopeLabel(scope), candidate.name, selected.name,
                      scopeLabel(selectedScope))},
            });
        } else {
            selected = candidate;
            selectedScope = scope;
        }
    }

    if (selected.profileId.isEmpty()) {
        return result;
    }

    result.insert(QStringLiteral("profileId"), selected.profileId);
    result.insert(QStringLiteral("name"), selected.name);
    result.insert(QStringLiteral("scope"), selectedScope);
    result.insert(QStringLiteral("contextKey"), contextKeys.value(selectedScope));
    result.insert(QStringLiteral("pinned"), selected.pinned);
    result.insert(QStringLiteral("reason"),
                 (selected.pinned
                      ? tr("Pinned via %1 scope binding: %2")
                      : tr("Selected via %1 scope binding: %2"))
                 .arg(scopeLabel(selectedScope), selected.name));
    result.insert(QStringLiteral("conflicts"), conflicts);
    return result;
}

QVariantMap EffectiveSettingsResolver::resolve(const QVariantMap& context) const
{
    QVariantMap values =
        s_BaselineProvider ? s_BaselineProvider() : QVariantMap();
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
    QVariantMap profileSelection;
    const QString hostUuid = context.value(QStringLiteral("hostUuid")).toString();
    const QString libraryEntryId =
        context.value(QStringLiteral("libraryEntryId")).toString();
    QString hostApplicationKey =
        context.value(QStringLiteral("hostApplicationKey")).toString();
    if (hostApplicationKey.isEmpty() && !hostUuid.isEmpty()
            && context.contains(QStringLiteral("appId"))) {
        hostApplicationKey = hostUuid + "|"
                + context.value(QStringLiteral("appId")).toString();
    }

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

        // Selected Streaming Profile applies next -- before the
        // Host-Client Pair / Library Entry / Host Application patches, so
        // those more specific patches can still override individual
        // profile fields (Baseline -> Streaming Profile -> Pair/Library/
        // Application patches -> Session Patch/Profile Pin -> Launch
        // Adaptation -> Capability Safety).
        profileSelection = selectStreamingProfile(
                    database, context, clientDeviceId, activeDisplayContextId,
                    activeDisplayContext, hostUuid, libraryEntryId,
                    hostApplicationKey);
        const QString selectedProfileId =
            profileSelection.value(QStringLiteral("profileId")).toString();
        if (!selectedProfileId.isEmpty()) {
            const QVariantMap profile = database->streamingProfile(selectedProfileId);
            QVariantMap profileValues = profile.value(QStringLiteral("values")).toMap();
            profileValues.remove(kScopeBindingsKey);
            QVariantMap profilePins;
            if (profileSelection.value(QStringLiteral("pinned")).toBool()) {
                for (auto it = profileValues.constBegin();
                     it != profileValues.constEnd(); ++it) {
                    profilePins.insert(it.key(), true);
                }
            }
            applyLayer(values, provenance, pins, floors,
                       profileValues, profilePins, {},
                       QStringLiteral("Streaming Profile"),
                       profileSelection.value(QStringLiteral("reason")).toString());
        }

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

        const QString pairKey = context.value(QStringLiteral("pairKey"),
                                               clientDeviceId + "|" + hostUuid).toString();
        applyPatch(QStringLiteral("host_client_pair"), pairKey,
                   QStringLiteral("Host–Client Pair"));
        applyPatch(QStringLiteral("library_entry"), libraryEntryId,
                   QStringLiteral("Library Entry"));
        applyPatch(QStringLiteral("host_application"), hostApplicationKey,
                   QStringLiteral("Host Application"));
    }

    applyLayer(values, provenance, pins, floors,
               context.value(QStringLiteral("sessionPatch")).toMap(),
               context.value(QStringLiteral("sessionPins")).toMap(),
               context.value(QStringLiteral("sessionFloors")).toMap(),
               QStringLiteral("Session Patch"),
               tr("Changed for this Session"));

    // Final capability safety is the only layer allowed to lower pinned
    // values -- and even then it must say so (pinConflicts) rather than
    // silently overriding a Profile Pin or per-field pin. Keep canonical
    // quality aliases alongside legacy preference keys until
    // StreamingPreferences' serialized names are retired.
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
    QVariantMap safe = s_CapabilitySafetyProvider
        ? s_CapabilitySafetyProvider(hostUuid, candidate)
        : candidate;
    for (auto it = candidate.constBegin(); it != candidate.constEnd(); ++it) {
        if (!safe.contains(it.key())) {
            safe.insert(it.key(), it.value());
        }
    }
    const QVariantMap safetyReasons = safe.value(QStringLiteral("reasons")).toMap();
    QVariantMap floorConflicts;
    QVariantMap pinConflicts;

    auto applySafety = [&](const QString& key, const QVariant& safeValue,
                           const QString& legacyKey) {
        const QVariant requested = candidate.value(key);
        const bool isPinned = pins.value(key, false).toBool();
        if (requested != safeValue) {
            QVariantMap field = provenance.value(key).toMap();
            if (field.isEmpty() && !legacyKey.isEmpty()) {
                field = provenance.value(legacyKey).toMap();
            }
            field.insert(QStringLiteral("value"), safeValue);
            field.insert(QStringLiteral("source"),
                         QStringLiteral("Capability Safety"));
            field.insert(QStringLiteral("reason"), safetyReasons.value(key));
            field.insert(QStringLiteral("pinned"), isPinned);
            provenance.insert(key, field);
            if (isPinned) {
                pinConflicts.insert(key, QVariantMap{
                    {QStringLiteral("requested"), requested},
                    {QStringLiteral("resolved"), safeValue},
                    {QStringLiteral("reason"), safetyReasons.value(key)},
                });
            }
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
        const bool hdrPinned = pins.value(QStringLiteral("hdr"), false).toBool();
        values.insert(QStringLiteral("hdr"), false);
        provenance.insert(QStringLiteral("hdr"), QVariantMap{
            {QStringLiteral("value"), false},
            {QStringLiteral("source"), QStringLiteral("Capability Safety")},
            {QStringLiteral("reason"),
             tr("The active display does not report HDR output capability")},
            {QStringLiteral("pinned"), hdrPinned},
        });
        if (hdrPinned) {
            pinConflicts.insert(QStringLiteral("hdr"), QVariantMap{
                {QStringLiteral("requested"), true},
                {QStringLiteral("resolved"), false},
                {QStringLiteral("reason"),
                 tr("The active display does not report HDR output capability")},
            });
        }
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
        {QStringLiteral("pinConflicts"), pinConflicts},
        {QStringLiteral("streamingProfile"), profileSelection},
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


QVariantList EffectiveSettingsResolver::streamingProfiles() const
{
    SettingsDatabase* database = SettingsDatabase::get();
    if (database == nullptr || !database->isOpen()) {
        return QVariantList();
    }
    QVariantList profiles = database->streamingProfiles();
    for (QVariant& entry : profiles) {
        QVariantMap profile = entry.toMap();
        QVariantMap values = profile.value(QStringLiteral("values")).toMap();
        profile.insert(QStringLiteral("scopeBindings"),
                       values.take(kScopeBindingsKey).toList());
        profile.insert(QStringLiteral("values"), values);
        entry = profile;
    }
    return profiles;
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
    QVariantMap persisted = values;
    if (!persisted.contains(kScopeBindingsKey) && !profileId.isEmpty()) {
        // Re-saving a Profile's quality values (e.g. from the Settings
        // page) must not silently drop its existing scope bindings.
        const QVariantMap existing = database->streamingProfile(profileId);
        const QVariant bindings = existing.value(QStringLiteral("values"))
                                          .toMap().value(kScopeBindingsKey);
        if (bindings.isValid()) {
            persisted.insert(kScopeBindingsKey, bindings);
        }
    }
    if (!database->saveStreamingProfile(id, name, persisted)) return {};
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

bool EffectiveSettingsResolver::bindStreamingProfileScope(
        const QString& profileId,
        const QString& scope,
        const QString& contextKey,
        bool pinned)
{
    SettingsDatabase* database = SettingsDatabase::get();
    if (database == nullptr || !database->isOpen() || profileId.isEmpty()
            || contextKey.isEmpty() || !kBindableScopes.contains(scope)) {
        return false;
    }
    if (scope == QStringLiteral("display_context")) {
        return setDisplayStreamingProfile(contextKey, profileId);
    }

    bool found = false;
    const QVariantList profiles = database->streamingProfiles();
    for (const QVariant& entry : profiles) {
        const QVariantMap record = entry.toMap();
        const QString otherId = record.value(QStringLiteral("id")).toString();
        QVariantMap values = record.value(QStringLiteral("values")).toMap();
        QVariantList bindings = values.value(kScopeBindingsKey).toList();
        bool changed = false;
        for (int i = bindings.size() - 1; i >= 0; --i) {
            const QVariantMap binding = bindings.at(i).toMap();
            if (binding.value(QStringLiteral("scope")).toString() == scope
                    && binding.value(QStringLiteral("key")).toString() == contextKey) {
                bindings.removeAt(i);
                changed = true;
            }
        }
        if (otherId == profileId) {
            bindings.append(QVariantMap{
                {QStringLiteral("scope"), scope},
                {QStringLiteral("key"), contextKey},
                {QStringLiteral("pinned"), pinned},
            });
            changed = true;
            found = true;
        }
        if (changed) {
            values.insert(kScopeBindingsKey, bindings);
            database->saveStreamingProfile(
                        otherId, record.value(QStringLiteral("name")).toString(),
                        values);
        }
    }
    if (found) {
        emit resolvedChanged({
            {QStringLiteral("scope"), scope},
            {QStringLiteral("contextKey"), contextKey},
        });
        emit streamingProfilesChanged();
    }
    return found;
}

bool EffectiveSettingsResolver::unbindStreamingProfileScope(
        const QString& scope,
        const QString& contextKey)
{
    SettingsDatabase* database = SettingsDatabase::get();
    if (database == nullptr || !database->isOpen() || contextKey.isEmpty()) {
        return false;
    }
    if (scope == QStringLiteral("display_context")) {
        return setDisplayStreamingProfile(contextKey, QString());
    }

    bool found = false;
    const QVariantList profiles = database->streamingProfiles();
    for (const QVariant& entry : profiles) {
        const QVariantMap record = entry.toMap();
        const QString otherId = record.value(QStringLiteral("id")).toString();
        QVariantMap values = record.value(QStringLiteral("values")).toMap();
        QVariantList bindings = values.value(kScopeBindingsKey).toList();
        bool changed = false;
        for (int i = bindings.size() - 1; i >= 0; --i) {
            const QVariantMap binding = bindings.at(i).toMap();
            if (binding.value(QStringLiteral("scope")).toString() == scope
                    && binding.value(QStringLiteral("key")).toString() == contextKey) {
                bindings.removeAt(i);
                changed = true;
                found = true;
            }
        }
        if (changed) {
            values.insert(kScopeBindingsKey, bindings);
            database->saveStreamingProfile(
                        otherId, record.value(QStringLiteral("name")).toString(),
                        values);
        }
    }
    if (found) {
        emit resolvedChanged({
            {QStringLiteral("scope"), scope},
            {QStringLiteral("contextKey"), contextKey},
        });
        emit streamingProfilesChanged();
    }
    return found;
}

QVariantList EffectiveSettingsResolver::streamingProfileScopeBindings(
        const QString& profileId) const
{
    SettingsDatabase* database = SettingsDatabase::get();
    if (database == nullptr || !database->isOpen() || profileId.isEmpty()) {
        return QVariantList();
    }
    const QVariantMap profile = database->streamingProfile(profileId);
    return profile.value(QStringLiteral("values")).toMap()
                  .value(kScopeBindingsKey).toList();
}
