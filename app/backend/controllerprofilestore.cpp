//
// SPDX-FileCopyrightText: Lunaframe Client Contributors
//
// SPDX-License-Identifier: GPL-3.0-only
//
#include "controllerprofilestore.h"
#include "core/settingsdatabase.h"
#include <QCryptographicHash>
#include <QUuid>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtGlobal>
#include <QDebug>

// --- ControllerCalibration --------------------------------------------------

QVariantMap
ControllerCalibration::toVariantMap() const
{
    QVariantMap map;
    map.insert(QStringLiteral("deadzoneLeftStick"), deadzoneLeftStick);
    map.insert(QStringLiteral("deadzoneRightStick"), deadzoneRightStick);
    map.insert(QStringLiteral("deadzoneLeftTrigger"), deadzoneLeftTrigger);
    map.insert(QStringLiteral("deadzoneRightTrigger"), deadzoneRightTrigger);
    map.insert(QStringLiteral("curveLeftStick"), curveLeftStick);
    map.insert(QStringLiteral("curveRightStick"), curveRightStick);
    map.insert(QStringLiteral("curveLeftTrigger"), curveLeftTrigger);
    map.insert(QStringLiteral("curveRightTrigger"), curveRightTrigger);
    return map;
}

ControllerCalibration
ControllerCalibration::fromVariantMap(const QVariantMap& map)
{
    ControllerCalibration cal;
    cal.deadzoneLeftStick = map.value(QStringLiteral("deadzoneLeftStick"), cal.deadzoneLeftStick).toDouble();
    cal.deadzoneRightStick = map.value(QStringLiteral("deadzoneRightStick"), cal.deadzoneRightStick).toDouble();
    cal.deadzoneLeftTrigger = map.value(QStringLiteral("deadzoneLeftTrigger"), cal.deadzoneLeftTrigger).toDouble();
    cal.deadzoneRightTrigger = map.value(QStringLiteral("deadzoneRightTrigger"), cal.deadzoneRightTrigger).toDouble();
    cal.curveLeftStick = map.value(QStringLiteral("curveLeftStick"), cal.curveLeftStick).toDouble();
    cal.curveRightStick = map.value(QStringLiteral("curveRightStick"), cal.curveRightStick).toDouble();
    cal.curveLeftTrigger = map.value(QStringLiteral("curveLeftTrigger"), cal.curveLeftTrigger).toDouble();
    cal.curveRightTrigger = map.value(QStringLiteral("curveRightTrigger"), cal.curveRightTrigger).toDouble();
    return cal;
}

// --- ControllerMap -------------------------------------------------------

QVariantMap
ControllerMap::toVariantMap() const
{
    QVariantMap map;
    map.insert(QStringLiteral("controllerPath"), controllerPath);
    map.insert(QStringLiteral("appId"), appId);
    map.insert(QStringLiteral("calibration"), calibration.toVariantMap());

    QVariantMap remap;
    for (auto it = buttonRemap.constBegin(); it != buttonRemap.constEnd(); ++it) {
        remap.insert(it.key(), it.value());
    }
    map.insert(QStringLiteral("buttonRemap"), remap);

    return map;
}

ControllerMap
ControllerMap::fromVariantMap(const QString& controllerPath, const QString& appId, const QVariantMap& map)
{
    ControllerMap profile;
    profile.controllerPath = controllerPath;
    profile.appId = appId;
    profile.calibration = ControllerCalibration::fromVariantMap(map.value(QStringLiteral("calibration")).toMap());

    QVariantMap remap = map.value(QStringLiteral("buttonRemap")).toMap();
    for (auto it = remap.constBegin(); it != remap.constEnd(); ++it) {
        profile.buttonRemap.insert(it.key(), it.value().toString());
    }

    return profile;
}

// --- JsonControllerMapBackend --------------------------------------------

namespace {
    const QString kControllerPrefix = QStringLiteral("controller:");
    const QString kGamePrefix = QStringLiteral("game:");
}

JsonControllerMapBackend::JsonControllerMapBackend(const QString& filePath)
    : m_FilePath(filePath.isEmpty() ? defaultFilePath() : filePath)
{
}

QString
JsonControllerMapBackend::defaultFilePath()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return dir + QStringLiteral("/controller-profiles.json");
}

QVariantMap
JsonControllerMapBackend::allProfiles() const
{
    return readAll().value(QStringLiteral("profiles")).toMap();
}

QString
JsonControllerMapBackend::key(const QString& controllerPath, const QString& appId)
{
    if (appId.isEmpty()) {
        return kControllerPrefix + controllerPath;
    }

    return kGamePrefix + appId + QStringLiteral("|") + controllerPath;
}

QVariantMap
JsonControllerMapBackend::readAll() const
{
    QFile file(m_FilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QVariantMap();
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();

    if (doc.isNull() || !doc.isObject()) {
        if (error.error != QJsonParseError::NoError) {
            qWarning() << "ControllerMapStore: malformed profile store" << m_FilePath
                       << "-" << error.errorString() << "; treating as empty";
        }
        return QVariantMap();
    }

    return doc.object().toVariantMap();
}

void
JsonControllerMapBackend::writeAll(const QVariantMap& root) const
{
    QFileInfo info(m_FilePath);
    QDir dir = info.dir();
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        qWarning() << "ControllerMapStore: failed to create" << dir.absolutePath();
        return;
    }

    QFile file(m_FilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "ControllerMapStore: failed to open" << m_FilePath << "for writing";
        return;
    }

    file.write(QJsonDocument(QJsonObject::fromVariantMap(root)).toJson(QJsonDocument::Indented));
    file.close();
}

QVariantMap
JsonControllerMapBackend::load(const QString& controllerPath, const QString& appId) const
{
    QVariantMap profiles = readAll().value(QStringLiteral("profiles")).toMap();
    return profiles.value(key(controllerPath, appId)).toMap();
}

void
JsonControllerMapBackend::save(const QString& controllerPath, const QString& appId, const QVariantMap& config)
{
    QVariantMap root = readAll();
    QVariantMap profiles = root.value(QStringLiteral("profiles")).toMap();
    profiles.insert(key(controllerPath, appId), config);
    root.insert(QStringLiteral("profiles"), profiles);
    root.insert(QStringLiteral("version"), 1);
    writeAll(root);
}

void
JsonControllerMapBackend::remove(const QString& controllerPath, const QString& appId)
{
    QVariantMap root = readAll();
    QVariantMap profiles = root.value(QStringLiteral("profiles")).toMap();
    profiles.remove(key(controllerPath, appId));
    root.insert(QStringLiteral("profiles"), profiles);
    writeAll(root);
}

QStringList
JsonControllerMapBackend::knownControllerPaths() const
{
    QVariantMap profiles = readAll().value(QStringLiteral("profiles")).toMap();

    QStringList paths;
    for (auto it = profiles.constBegin(); it != profiles.constEnd(); ++it) {
        if (it.key().startsWith(kControllerPrefix)) {
            paths.append(it.key().mid(kControllerPrefix.length()));
        }
    }

    return paths;
}

// --- SqliteControllerMapBackend -------------------------------------------

QString
SqliteControllerMapBackend::scopeForContext(const QString& contextKey)
{
    if (contextKey.isEmpty()) return QStringLiteral("controller");
    const qsizetype separator = contextKey.indexOf(QLatin1Char(':'));
    return separator > 0 ? contextKey.left(separator)
                         : QStringLiteral("host_application");
}

QString
SqliteControllerMapBackend::keyWithoutScope(const QString& contextKey)
{
    const qsizetype separator = contextKey.indexOf(QLatin1Char(':'));
    return separator > 0 ? contextKey.mid(separator + 1) : contextKey;
}

QVariantMap
SqliteControllerMapBackend::load(const QString& controllerId,
                                 const QString& contextKey) const
{
    SettingsDatabase* database = SettingsDatabase::get();
    return database
        ? database->controllerMap(controllerId,
                                  scopeForContext(contextKey),
                                  keyWithoutScope(contextKey))
        : QVariantMap();
}

void
SqliteControllerMapBackend::save(const QString& controllerId,
                                 const QString& contextKey,
                                 const QVariantMap& config)
{
    if (SettingsDatabase* database = SettingsDatabase::get()) {
        database->setControllerMap(controllerId,
                                   scopeForContext(contextKey),
                                   keyWithoutScope(contextKey),
                                   config);
    }
}

void
SqliteControllerMapBackend::remove(const QString& controllerId,
                                   const QString& contextKey)
{
    if (SettingsDatabase* database = SettingsDatabase::get()) {
        database->removeControllerMap(controllerId,
                                      scopeForContext(contextKey),
                                      keyWithoutScope(contextKey));
    }
}

QStringList
SqliteControllerMapBackend::knownControllerPaths() const
{
    SettingsDatabase* database = SettingsDatabase::get();
    return database ? database->knownControllerIds() : QStringList();
}

// --- ControllerMapStore ---------------------------------------------------

namespace {
constexpr int kMaxPlayerSlots = 16;
QVariantMap mergeMap(const QVariantMap& base, const QVariantMap& patch)
{
    QVariantMap merged = base;
    for (auto it = patch.constBegin(); it != patch.constEnd(); ++it) {
        if ((it.key() == QStringLiteral("calibration")
             || it.key() == QStringLiteral("buttonRemap"))
                && it.value().canConvert<QVariantMap>()) {
            QVariantMap nested = merged.value(it.key()).toMap();
            const QVariantMap nestedPatch = it.value().toMap();
            for (auto nestedIt = nestedPatch.constBegin();
                 nestedIt != nestedPatch.constEnd(); ++nestedIt) {
                nested.insert(nestedIt.key(), nestedIt.value());
            }
            merged.insert(it.key(), nested);
        } else {
            merged.insert(it.key(), it.value());
        }
    }
    return merged;
}

QString scopedContext(const QString& scope, const QString& contextKey)
{
    return scope == QStringLiteral("controller")
        ? QString()
        : scope + QLatin1Char(':') + contextKey;
}

QString playerSlotKey(const QString& controllerId)
{
    return QStringLiteral("controller.player_slot.")
        + QString::fromLatin1(
            QCryptographicHash::hash(controllerId.toUtf8(),
                                     QCryptographicHash::Sha256).toHex());
}
}

ControllerMapStore* ControllerMapStore::s_Instance = nullptr;

ControllerMapStore*
ControllerMapStore::get()
{
    if (s_Instance == nullptr) {
        if (SettingsDatabase* database = SettingsDatabase::get()) {
            JsonControllerMapBackend legacy;
            database->importLegacyControllerMaps(
                legacy.allProfiles(),
                QStringLiteral("migration.controller_maps_json_v1"));
        }
        s_Instance = new ControllerMapStore(new SqliteControllerMapBackend());
    }
    return s_Instance;
}

ControllerMapStore::ControllerMapStore(IControllerMapBackend* backend,
                                       QObject* parent)
    : QObject(parent)
    , m_Backend(backend)
{
    Q_ASSERT(m_Backend != nullptr);
}

ControllerMapStore::~ControllerMapStore()
{
    delete m_Backend;
}

QString
ControllerMapStore::controllerId(SDL_GameController* controller)
{
    if (controller == nullptr) return {};
#if SDL_VERSION_ATLEAST(2, 24, 0)
    const char* path = SDL_GameControllerPath(controller);
    if (path != nullptr && path[0] != '\0') {
        return QString::fromUtf8(path);
    }
#endif

    SDL_Joystick* joystick = SDL_GameControllerGetJoystick(controller);
    char guidText[64] = {};
    SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(joystick),
                              guidText,
                              sizeof(guidText));
#if SDL_VERSION_ATLEAST(2, 0, 14)
    const char* serial = SDL_GameControllerGetSerial(controller);
    if (serial != nullptr && serial[0] != '\0') {
        return QStringLiteral("serial:")
            + QString::fromLatin1(guidText) + QLatin1Char(':')
            + QString::fromUtf8(serial);
    }
#endif

    const QString name = QString::fromUtf8(
        SDL_GameControllerName(controller)
            ? SDL_GameControllerName(controller)
            : "unknown");
    const QString fingerprint =
        QStringLiteral("%1|%2|%3|%4")
            .arg(name)
            .arg(SDL_GameControllerGetVendor(controller))
            .arg(SDL_GameControllerGetProduct(controller))
            .arg(QString::fromLatin1(guidText));
    const QString fingerprintHash = QString::fromLatin1(
        QCryptographicHash::hash(fingerprint.toUtf8(),
                                 QCryptographicHash::Sha256).toHex());
    const QString settingKey =
        QStringLiteral("controller.identity.") + fingerprintHash;
    if (SettingsDatabase* database = SettingsDatabase::get()) {
        QString minted = database->setting(settingKey).toString();
        if (minted.isEmpty()) {
            minted = QUuid::createUuid().toString(QUuid::WithoutBraces);
            database->setSetting(settingKey, minted);
        }
        return QStringLiteral("minted:") + minted;
    }
    return QStringLiteral("minted:") + fingerprintHash;
}

QVariantMap
ControllerMapStore::mapFor(const QString& controllerId,
                           const QString& libraryEntryId,
                           const QString& hostApplicationKey) const
{
    ControllerMap defaults;
    defaults.controllerPath = controllerId;
    QVariantMap effective = defaults.toVariantMap();
    effective = mergeMap(effective, m_Backend->load(controllerId, QString()));

    if (!libraryEntryId.isEmpty()) {
        effective = mergeMap(
            effective,
            m_Backend->load(controllerId,
                            QStringLiteral("library_entry:") + libraryEntryId));
    }
    if (!hostApplicationKey.isEmpty()) {
        const qsizetype separator =
            hostApplicationKey.lastIndexOf(QLatin1Char('|'));
        if (separator >= 0 && separator + 1 < hostApplicationKey.size()) {
            effective = mergeMap(
                effective,
                m_Backend->load(
                    controllerId,
                    QStringLiteral("host_application:")
                        + hostApplicationKey.mid(separator + 1)));
        }
        effective = mergeMap(
            effective,
            m_Backend->load(controllerId,
                            QStringLiteral("host_application:")
                                + hostApplicationKey));
    }

    ControllerMap normalized =
        ControllerMap::fromVariantMap(controllerId,
                                      hostApplicationKey,
                                      effective);
    return normalized.toVariantMap();
}

void
ControllerMapStore::saveMap(const QString& controllerId,
                            const QString& scope,
                            const QString& contextKey,
                            const QVariantMap& patch)
{
    if (controllerId.isEmpty()
            || (scope != QStringLiteral("controller")
                && scope != QStringLiteral("library_entry")
                && scope != QStringLiteral("host_application"))
            || (scope != QStringLiteral("controller")
                && contextKey.isEmpty())) {
        qWarning() << "ControllerMapStore: refusing invalid map scope"
                   << controllerId << scope << contextKey;
        return;
    }
    m_Backend->save(controllerId, scopedContext(scope, contextKey), patch);
    emit mapChanged(controllerId, scope, contextKey);
}

void
ControllerMapStore::resetMap(const QString& controllerId,
                             const QString& scope,
                             const QString& contextKey)
{
    m_Backend->remove(controllerId, scopedContext(scope, contextKey));
    emit mapChanged(controllerId, scope, contextKey);
}

QStringList
ControllerMapStore::knownControllerIds() const
{
    return m_Backend->knownControllerPaths();
}

int
ControllerMapStore::playerSlot(const QString& controllerId) const
{
    SettingsDatabase* database = SettingsDatabase::get();
    return database
        ? database->setting(playerSlotKey(controllerId), -1).toInt()
        : -1;
}

bool
ControllerMapStore::setPlayerSlot(const QString& controllerId, int slot)
{
    SettingsDatabase* database = SettingsDatabase::get();
    if (database == nullptr || controllerId.isEmpty()
            || slot < 0 || slot >= kMaxPlayerSlots) {
        return false;
    }
    database->setSetting(playerSlotKey(controllerId), slot);
    return database->setting(playerSlotKey(controllerId), -1).toInt() == slot;
}
