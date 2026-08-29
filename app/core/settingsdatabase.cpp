#include "settingsdatabase.h"

#include <QAtomicInteger>
#include <QDateTime>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QtDebug>

namespace {
    // Every SettingsDatabase instance gets its own QSqlDatabase connection
    // name so multiple instances (e.g. in tests, or a fresh instance from a
    // re-run open()) never collide in Qt's global connection registry.
    QAtomicInteger<quint64> s_ConnectionCounter(0);
}

    QString mapToJson(const QVariantMap& value)
    {
        return QString::fromUtf8(
                    QJsonDocument::fromVariant(value).toJson(QJsonDocument::Compact));
    }

    QVariantMap jsonToMap(const QVariant& value)
    {
        return QJsonDocument::fromJson(value.toString().toUtf8())
                .object().toVariantMap();
    }

SettingsDatabase* SettingsDatabase::s_Instance = nullptr;

SettingsDatabase* SettingsDatabase::get()
{
    return s_Instance;
}

SettingsDatabase::SettingsDatabase(QObject* parent)
    : QObject(parent)
{
    // Jochona: last constructed instance is the process-wide one (created at
    // startup in main.cpp). Tests create short-lived instances that take over
    // and release via the destructor.
    s_Instance = this;
}

SettingsDatabase::~SettingsDatabase()
{
    if (s_Instance == this) {
        s_Instance = nullptr;
    }
}
void SettingsDatabase::closeConnection()
{
    if (m_Db.isOpen()) {
        m_Db.close();
    }

    // Drop our reference to the QSqlDatabase before removing it from the
    // registry; QSqlDatabase::removeDatabase() warns if any copies remain.
    m_Db = QSqlDatabase();

    if (!m_ConnectionName.isEmpty()) {
        QSqlDatabase::removeDatabase(m_ConnectionName);
        m_ConnectionName.clear();
    }
}

bool SettingsDatabase::open(const QString& databasePath)
{
    closeConnection();

    const QString path = databasePath.isEmpty() ? defaultDatabasePath() : databasePath;

    m_ConnectionName = QStringLiteral("SettingsDatabase-%1").arg(s_ConnectionCounter.fetchAndAddOrdered(1));
    m_Db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_ConnectionName);
    m_Db.setDatabaseName(path);

    if (!m_Db.open()) {
        setLastError(QStringLiteral("Failed to open database at %1: %2").arg(path, m_Db.lastError().text()));
        closeConnection();
        return false;
    }

    QSqlQuery pragma(m_Db);
    if (!pragma.exec(QStringLiteral("PRAGMA journal_mode = WAL")) ||
            !pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
        setLastError(QStringLiteral("Failed to configure database at %1: %2").arg(path, m_Db.lastError().text()));
        closeConnection();
        return false;
    }

    if (!ensureMigrationsTable()) {
        closeConnection();
        return false;
    }

    const int current = currentSchemaVersion();
    const int known = latestKnownSchemaVersion();
    if (current > known) {
        setLastError(QStringLiteral("Database schema version %1 at %2 is newer than the version %3 this build "
                                     "understands; refusing to open it to avoid silent data loss")
                             .arg(current)
                             .arg(path)
                             .arg(known));
        closeConnection();
        return false;
    }

    for (const Migration& migration : migrations()) {
        if (migration.version <= current) {
            continue;
        }

        if (!backupBeforeMigration(migration.version) || !applyMigration(migration)) {
            closeConnection();
            return false;
        }
    }

    m_DatabasePath = path;
    m_LastError.clear();
    return true;
}

bool SettingsDatabase::isOpen() const
{
    return m_Db.isOpen();
}

int SettingsDatabase::schemaVersion() const
{
    return m_Db.isOpen() ? currentSchemaVersion() : 0;
}

QString SettingsDatabase::databasePath() const
{
    return m_DatabasePath;
}

QString SettingsDatabase::lastError() const
{
    return m_LastError;
}

void SettingsDatabase::setLastError(const QString& error)
{
    m_LastError = error;
    if (!error.isEmpty()) {
        qWarning() << "SettingsDatabase:" << error;
    }
}

bool SettingsDatabase::ensureMigrationsTable()
{
    QSqlQuery query(m_Db);
    if (!query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS migrations ("
                                    "version INTEGER PRIMARY KEY,"
                                    "applied_at TEXT NOT NULL"
                                    ")"))) {
        setLastError(QStringLiteral("Failed to create migrations table: %1").arg(query.lastError().text()));
        return false;
    }
    return true;
}

int SettingsDatabase::currentSchemaVersion() const
{
    QSqlQuery query(m_Db);
    if (!query.exec(QStringLiteral("SELECT COALESCE(MAX(version), 0) FROM migrations")) || !query.next()) {
        return 0;
    }
    return query.value(0).toInt();
}

bool SettingsDatabase::backupBeforeMigration(int version)
{
    const QString backupPath = QStringLiteral("%1.bak-%2").arg(m_Db.databaseName()).arg(version);

    // VACUUM INTO refuses to overwrite an existing file, so clear out any
    // stale backup left behind by a previous interrupted migration attempt.
    QFile::remove(backupPath);

    QSqlQuery query(m_Db);
    query.prepare(QStringLiteral("VACUUM INTO ?"));
    query.addBindValue(backupPath);
    if (!query.exec()) {
        setLastError(QStringLiteral("Failed to snapshot database before migration %1: %2")
                             .arg(version)
                             .arg(query.lastError().text()));
        return false;
    }
    return true;
}

bool SettingsDatabase::applyMigration(const Migration& migration)
{
    if (!m_Db.transaction()) {
        setLastError(QStringLiteral("Failed to start transaction for migration %1: %2")
                             .arg(migration.version)
                             .arg(m_Db.lastError().text()));
        return false;
    }

    QSqlQuery query(m_Db);
    for (const QString& statement : migration.statements) {
        if (!query.exec(statement)) {
            setLastError(QStringLiteral("Migration %1 (%2) failed: %3")
                                 .arg(migration.version)
                                 .arg(migration.description, query.lastError().text()));
            m_Db.rollback();
            return false;
        }
    }

    query.prepare(QStringLiteral("INSERT INTO migrations (version, applied_at) VALUES (?, ?)"));
    query.addBindValue(migration.version);
    query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    if (!query.exec()) {
        setLastError(QStringLiteral("Failed to record migration %1: %2")
                             .arg(migration.version)
                             .arg(query.lastError().text()));
        m_Db.rollback();
        return false;
    }

    if (!m_Db.commit()) {
        setLastError(QStringLiteral("Failed to commit migration %1: %2")
                             .arg(migration.version)
                             .arg(m_Db.lastError().text()));
        m_Db.rollback();
        return false;
    }

    return true;
}

QVariant SettingsDatabase::setting(const QString& key, const QVariant& defaultValue) const
{
    if (!m_Db.isOpen()) {
        return defaultValue;
    }

    QSqlQuery query(m_Db);
    query.prepare(QStringLiteral("SELECT value FROM settings WHERE key = ?"));
    query.addBindValue(key);
    if (!query.exec() || !query.next()) {
        return defaultValue;
    }
    return query.value(0);
}

void SettingsDatabase::setSetting(const QString& key, const QVariant& value)
{
    if (!m_Db.isOpen()) {
        return;
    }

    QSqlQuery query(m_Db);
    query.prepare(QStringLiteral("INSERT INTO settings (key, value) VALUES (?, ?) "
                                  "ON CONFLICT(key) DO UPDATE SET value = excluded.value"));
    query.addBindValue(key);
    query.addBindValue(value);
    if (!query.exec()) {
        setLastError(QStringLiteral("Failed to write setting %1: %2").arg(key, query.lastError().text()));
    }
}

bool SettingsDatabase::setSettings(const QVariantMap& values)
{
    if (!m_Db.isOpen()) {
        return false;
    }
    if (!m_Db.transaction()) {
        setLastError(QStringLiteral("Failed to begin settings transaction: %1")
                     .arg(m_Db.lastError().text()));
        return false;
    }

    QSqlQuery query(m_Db);
    query.prepare(QStringLiteral("INSERT INTO settings (key, value) VALUES (?, ?) "
                                 "ON CONFLICT(key) DO UPDATE SET value = excluded.value"));
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        query.bindValue(0, it.key());
        query.bindValue(1, it.value());
        if (!query.exec()) {
            m_Db.rollback();
            setLastError(QStringLiteral("Failed to write setting %1: %2")
                         .arg(it.key(), query.lastError().text()));
            return false;
        }
    }
    if (!m_Db.commit()) {
        setLastError(QStringLiteral("Failed to commit settings transaction: %1")
                     .arg(m_Db.lastError().text()));
        return false;
    }
    setLastError(QString());
    return true;
}

bool SettingsDatabase::importLegacySettings(const QVariantMap& values,
                                            const QString& markerKey)
{
    if (!m_Db.isOpen()) {
        return false;
    }
    if (setting(markerKey, false).toBool()) {
        return true;
    }
    QVariantMap importValues = values;
    importValues.insert(markerKey, true);
    return setSettings(importValues);
}

QVariantMap SettingsDatabase::settingsPatch(const QString& scope,
                                            const QString& contextKey) const
{
    if (!m_Db.isOpen()) {
        return {};
    }
    QSqlQuery query(m_Db);
    query.prepare(QStringLiteral(
        "SELECT values_json, pins_json, floors_json FROM settings_patches "
        "WHERE scope = ? AND context_key = ?"));
    query.addBindValue(scope);
    query.addBindValue(contextKey);
    if (!query.exec() || !query.next()) {
        return {};
    }
    return {
        {QStringLiteral("values"), jsonToMap(query.value(0))},
        {QStringLiteral("pins"), jsonToMap(query.value(1))},
        {QStringLiteral("floors"), jsonToMap(query.value(2))},
    };
}

bool SettingsDatabase::setSettingsPatch(const QString& scope,
                                        const QString& contextKey,
                                        const QVariantMap& values,
                                        const QVariantMap& pins,
                                        const QVariantMap& floors)
{
    if (!m_Db.isOpen()) {
        return false;
    }
    QSqlQuery query(m_Db);
    query.prepare(QStringLiteral(
        "INSERT INTO settings_patches "
        "(scope, context_key, values_json, pins_json, floors_json, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(scope, context_key) DO UPDATE SET "
        "values_json = excluded.values_json, pins_json = excluded.pins_json, "
        "floors_json = excluded.floors_json, updated_at = excluded.updated_at"));
    query.addBindValue(scope);
    query.addBindValue(contextKey);
    query.addBindValue(mapToJson(values));
    query.addBindValue(mapToJson(pins));
    query.addBindValue(mapToJson(floors));
    query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    if (!query.exec()) {
        setLastError(QStringLiteral("Failed to save %1 patch %2: %3")
                     .arg(scope, contextKey, query.lastError().text()));
        return false;
    }
    return true;
}

QVariantMap SettingsDatabase::streamingProfile(const QString& profileId) const
{
    if (!m_Db.isOpen() || profileId.isEmpty()) {
        return {};
    }
    QSqlQuery query(m_Db);
    query.prepare(QStringLiteral(
        "SELECT name, values_json FROM streaming_profiles WHERE id = ?"));
    query.addBindValue(profileId);
    if (!query.exec() || !query.next()) {
        return {};
    }
    return {
        {QStringLiteral("id"), profileId},
        {QStringLiteral("name"), query.value(0).toString()},
        {QStringLiteral("values"), jsonToMap(query.value(1))},
    };
}

bool SettingsDatabase::saveStreamingProfile(const QString& profileId,
                                            const QString& name,
                                            const QVariantMap& values)
{
    if (!m_Db.isOpen() || profileId.isEmpty() || name.trimmed().isEmpty()) {
        return false;
    }
    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    QSqlQuery query(m_Db);
    query.prepare(QStringLiteral(
        "INSERT INTO streaming_profiles "
        "(id, name, values_json, created_at, updated_at) VALUES (?, ?, ?, ?, ?) "
        "ON CONFLICT(id) DO UPDATE SET name = excluded.name, "
        "values_json = excluded.values_json, updated_at = excluded.updated_at"));
    query.addBindValue(profileId);
    query.addBindValue(name.trimmed());
    query.addBindValue(mapToJson(values));
    query.addBindValue(now);
    query.addBindValue(now);
    if (!query.exec()) {
        setLastError(QStringLiteral("Failed to save Streaming Profile %1: %2")
                     .arg(profileId, query.lastError().text()));
        return false;
    }
    return true;
}

QVariantList SettingsDatabase::streamingProfiles() const
{
    QVariantList profiles;
    if (!m_Db.isOpen()) return profiles;
    QSqlQuery query(QStringLiteral(
        "SELECT id,name,values_json,updated_at "
        "FROM streaming_profiles ORDER BY name COLLATE NOCASE"), m_Db);
    while (query.next()) {
        profiles.append(QVariantMap{
            {QStringLiteral("id"), query.value(0)},
            {QStringLiteral("name"), query.value(1)},
            {QStringLiteral("values"), jsonToMap(query.value(2))},
            {QStringLiteral("updatedAt"), query.value(3)},
        });
    }
    return profiles;
}

bool SettingsDatabase::deleteStreamingProfile(const QString& profileId)
{
    if (!m_Db.isOpen() || profileId.isEmpty()) return false;
    if (!m_Db.transaction()) return false;
    QSqlQuery contexts(m_Db);
    contexts.prepare(QStringLiteral(
        "UPDATE display_contexts SET streaming_profile_id=NULL "
        "WHERE streaming_profile_id=?"));
    contexts.addBindValue(profileId);
    QSqlQuery profile(m_Db);
    profile.prepare(QStringLiteral(
        "DELETE FROM streaming_profiles WHERE id=?"));
    profile.addBindValue(profileId);
    if (!contexts.exec() || !profile.exec() || !m_Db.commit()) {
        m_Db.rollback();
        return false;
    }
    return profile.numRowsAffected() == 1;
}

bool SettingsDatabase::ensureClientDevice(const QString& deviceId,
                                          const QString& name)
{
    if (!m_Db.isOpen() || deviceId.isEmpty()) {
        return false;
    }
    QSqlQuery query(m_Db);
    query.prepare(QStringLiteral(
        "INSERT INTO client_devices (id, name, created_at) VALUES (?, ?, ?) "
        "ON CONFLICT(id) DO UPDATE SET name = excluded.name"));
    query.addBindValue(deviceId);
    query.addBindValue(name.isEmpty() ? QStringLiteral("This device") : name);
    query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    if (!query.exec()) {
        setLastError(QStringLiteral("Failed to store Client Device %1: %2")
                     .arg(deviceId, query.lastError().text()));
        return false;
    }
    return true;
}

QVariantMap
SettingsDatabase::clientDeviceSettings(const QString& deviceId) const
{
    if (!m_Db.isOpen()) return {};
    QSqlQuery query(m_Db);
    query.prepare(QStringLiteral(
        "SELECT values_json,pins_json,floors_json "
        "FROM client_devices WHERE id=?"));
    query.addBindValue(deviceId);
    if (!query.exec() || !query.next()) return {};
    return {
        {QStringLiteral("values"), jsonToMap(query.value(0))},
        {QStringLiteral("pins"), jsonToMap(query.value(1))},
        {QStringLiteral("floors"), jsonToMap(query.value(2))},
    };
}

bool
SettingsDatabase::setClientDeviceSettings(const QString& deviceId,
                                          const QVariantMap& values,
                                          const QVariantMap& pins,
                                          const QVariantMap& floors)
{
    if (!m_Db.isOpen() || deviceId.isEmpty()) return false;
    QSqlQuery query(m_Db);
    query.prepare(QStringLiteral(
        "UPDATE client_devices SET values_json=?,pins_json=?,floors_json=? "
        "WHERE id=?"));
    query.addBindValue(mapToJson(values));
    query.addBindValue(mapToJson(pins));
    query.addBindValue(mapToJson(floors));
    query.addBindValue(deviceId);
    return query.exec() && query.numRowsAffected() == 1;
}

QVariantMap
SettingsDatabase::displayContext(const QString& contextId) const
{
    if (!m_Db.isOpen() || contextId.isEmpty()) return {};
    QSqlQuery query(m_Db);
    query.prepare(QStringLiteral(
        "SELECT name,fingerprint,dock_state,streaming_profile_id,"
        "values_json,pins_json,floors_json,width,height,refresh_hz,"
        "hdr_capable,last_seen FROM display_contexts WHERE id=?"));
    query.addBindValue(contextId);
    if (!query.exec() || !query.next()) return {};
    return {
        {QStringLiteral("id"), contextId},
        {QStringLiteral("name"), query.value(0)},
        {QStringLiteral("fingerprint"), query.value(1)},
        {QStringLiteral("dockState"), query.value(2)},
        {QStringLiteral("profileId"), query.value(3)},
        {QStringLiteral("values"), jsonToMap(query.value(4))},
        {QStringLiteral("pins"), jsonToMap(query.value(5))},
        {QStringLiteral("floors"), jsonToMap(query.value(6))},
        {QStringLiteral("width"), query.value(7)},
        {QStringLiteral("height"), query.value(8)},
        {QStringLiteral("refreshHz"), query.value(9)},
        {QStringLiteral("hdrCapable"), query.value(10).toBool()},
        {QStringLiteral("lastSeen"), query.value(11)},
    };
}

bool
SettingsDatabase::upsertDisplayContext(const QString& contextId,
                                       const QString& deviceId,
                                       const QString& name,
                                       const QString& fingerprint,
                                       const QString& dockState,
                                       const QVariantMap& metadata)
{
    if (!m_Db.isOpen() || contextId.isEmpty() || deviceId.isEmpty()) {
        return false;
    }
    QSqlQuery query(m_Db);
    query.prepare(QStringLiteral(
        "INSERT INTO display_contexts "
        "(id,client_device_id,name,fingerprint,dock_state,width,height,"
        "refresh_hz,hdr_capable,last_seen) VALUES (?,?,?,?,?,?,?,?,?,?) "
        "ON CONFLICT(id) DO UPDATE SET name=excluded.name,"
        "fingerprint=excluded.fingerprint,dock_state=excluded.dock_state,"
        "width=excluded.width,height=excluded.height,"
        "refresh_hz=excluded.refresh_hz,hdr_capable=excluded.hdr_capable,"
        "last_seen=excluded.last_seen"));
    query.addBindValue(contextId);
    query.addBindValue(deviceId);
    query.addBindValue(name);
    query.addBindValue(fingerprint);
    query.addBindValue(dockState);
    query.addBindValue(metadata.value(QStringLiteral("width"), 0));
    query.addBindValue(metadata.value(QStringLiteral("height"), 0));
    query.addBindValue(metadata.value(QStringLiteral("refreshHz"), 0.0));
    query.addBindValue(metadata.value(QStringLiteral("hdrCapable"), false));
    query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    return query.exec();
}

bool
SettingsDatabase::setDisplayContextSettings(const QString& contextId,
                                            const QVariantMap& values,
                                            const QVariantMap& pins,
                                            const QVariantMap& floors,
                                            const QString& profileId)
{
    if (!m_Db.isOpen() || contextId.isEmpty()) return false;
    QSqlQuery query(m_Db);
    query.prepare(QStringLiteral(
        "UPDATE display_contexts SET values_json=?,pins_json=?,floors_json=?,"
        "streaming_profile_id=? WHERE id=?"));
    query.addBindValue(mapToJson(values));
    query.addBindValue(mapToJson(pins));
    query.addBindValue(mapToJson(floors));
    query.addBindValue(profileId.isEmpty() ? QVariant() : profileId);
    query.addBindValue(contextId);
    return query.exec() && query.numRowsAffected() == 1;
}

QVariantMap SettingsDatabase::controllerMap(const QString& controllerId,
                                            const QString& scope,
                                            const QString& contextKey) const
{
    if (!m_Db.isOpen()) return {};
    QSqlQuery query(m_Db);
    query.prepare(QStringLiteral(
        "SELECT map_json FROM controller_maps "
        "WHERE controller_id=? AND scope=? AND context_key=?"));
    query.addBindValue(controllerId);
    query.addBindValue(scope);
    query.addBindValue(contextKey.isNull() ? QStringLiteral("")
                                           : contextKey);
    return query.exec() && query.next() ? jsonToMap(query.value(0)) : QVariantMap();
}

bool SettingsDatabase::setControllerMap(const QString& controllerId,
                                        const QString& scope,
                                        const QString& contextKey,
                                        const QVariantMap& map)
{
    if (!m_Db.isOpen() || controllerId.isEmpty()) return false;
    QSqlQuery query(m_Db);
    query.prepare(QStringLiteral(
        "INSERT INTO controller_maps "
        "(controller_id,scope,context_key,map_json,updated_at) VALUES (?,?,?,?,?) "
        "ON CONFLICT(controller_id,scope,context_key) DO UPDATE SET "
        "map_json=excluded.map_json,updated_at=excluded.updated_at"));
    query.addBindValue(controllerId);
    query.addBindValue(scope);
    query.addBindValue(contextKey.isNull() ? QStringLiteral("")
                                           : contextKey);
    query.addBindValue(mapToJson(map));
    query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    return query.exec();
}

bool SettingsDatabase::removeControllerMap(const QString& controllerId,
                                           const QString& scope,
                                           const QString& contextKey)
{
    if (!m_Db.isOpen()) return false;
    QSqlQuery query(m_Db);
    query.prepare(QStringLiteral(
        "DELETE FROM controller_maps WHERE controller_id=? AND scope=? AND context_key=?"));
    query.addBindValue(controllerId);
    query.addBindValue(scope);
    query.addBindValue(contextKey.isNull() ? QStringLiteral("")
                                           : contextKey);
    return query.exec();
}

QStringList SettingsDatabase::knownControllerIds() const
{
    QStringList ids;
    if (!m_Db.isOpen()) return ids;
    QSqlQuery query(QStringLiteral(
        "SELECT DISTINCT controller_id FROM controller_maps ORDER BY controller_id"), m_Db);
    while (query.next()) ids.append(query.value(0).toString());
    return ids;
}

QVariantMap SettingsDatabase::capabilityCache() const
{
    QVariantMap cache;
    if (!m_Db.isOpen()) return cache;
    QSqlQuery query(QStringLiteral(
        "SELECT host_id,capabilities_json,confidence,verified_at "
        "FROM capability_cache"), m_Db);
    while (query.next()) {
        QVariantMap capability = jsonToMap(query.value(1));
        capability.insert(QStringLiteral("confidence"), query.value(2));
        capability.insert(QStringLiteral("lastProbed"), query.value(3));
        cache.insert(query.value(0).toString(), capability);
    }
    return cache;
}

bool SettingsDatabase::setCapability(const QString& hostId,
                                     const QVariantMap& capabilities,
                                     const QString& confidence,
                                     const QDateTime& verifiedAt)
{
    if (!m_Db.isOpen() || hostId.isEmpty()) return false;
    QSqlQuery host(m_Db);
    host.prepare(QStringLiteral(
        "INSERT INTO hosts(id,name,updated_at) VALUES (?,?,?) "
        "ON CONFLICT(id) DO NOTHING"));
    host.addBindValue(hostId);
    host.addBindValue(QStringLiteral("Unknown Host"));
    host.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    if (!host.exec()) return false;

    QSqlQuery query(m_Db);
    query.prepare(QStringLiteral(
        "INSERT INTO capability_cache "
        "(host_id,capabilities_json,confidence,verified_at) VALUES (?,?,?,?) "
        "ON CONFLICT(host_id) DO UPDATE SET "
        "capabilities_json=excluded.capabilities_json,"
        "confidence=excluded.confidence,verified_at=excluded.verified_at"));
    query.addBindValue(hostId);
    query.addBindValue(mapToJson(capabilities));
    query.addBindValue(confidence);
    query.addBindValue(verifiedAt.isValid()
                       ? verifiedAt.toUTC().toString(Qt::ISODateWithMs)
                       : QDateTime::currentDateTimeUtc().toString(
                             Qt::ISODateWithMs));
    return query.exec();
}

bool SettingsDatabase::importLegacyCapabilities(
        const QVariantMap& capabilities,
        const QString& markerKey)
{
    if (!m_Db.isOpen()) return false;
    if (setting(markerKey, false).toBool()) return true;
    if (!m_Db.transaction()) return false;
    for (auto it = capabilities.constBegin();
         it != capabilities.constEnd(); ++it) {
        const QVariantMap capability = it.value().toMap();
        QSqlQuery host(m_Db);
        host.prepare(QStringLiteral(
            "INSERT INTO hosts(id,name,updated_at) VALUES (?,?,?) "
            "ON CONFLICT(id) DO NOTHING"));
        host.addBindValue(it.key());
        host.addBindValue(QStringLiteral("Unknown Host"));
        host.addBindValue(QDateTime::currentDateTimeUtc().toString(
                              Qt::ISODateWithMs));
        if (!host.exec()) {
            m_Db.rollback();
            return false;
        }
        QSqlQuery cache(m_Db);
        cache.prepare(QStringLiteral(
            "INSERT INTO capability_cache "
            "(host_id,capabilities_json,confidence,verified_at) "
            "VALUES (?,?,?,?) ON CONFLICT(host_id) DO NOTHING"));
        cache.addBindValue(it.key());
        cache.addBindValue(mapToJson(capability));
        cache.addBindValue(capability.value(
                               QStringLiteral("confidence"),
                               QStringLiteral("unknown")));
        cache.addBindValue(capability.value(
                               QStringLiteral("lastProbed"),
                               QDateTime::currentDateTimeUtc().toString(
                                   Qt::ISODateWithMs)));
        if (!cache.exec()) {
            m_Db.rollback();
            return false;
        }
    }
    QSqlQuery marker(m_Db);
    marker.prepare(QStringLiteral(
        "INSERT INTO settings(key,value) VALUES (?,1) "
        "ON CONFLICT(key) DO UPDATE SET value=1"));
    marker.addBindValue(markerKey);
    if (!marker.exec() || !m_Db.commit()) {
        m_Db.rollback();
        return false;
    }
    return true;
}

QVariantList SettingsDatabase::beacons() const
{
    QVariantList rows;
    if (!m_Db.isOpen()) return rows;
    QSqlQuery query(QStringLiteral(
        "SELECT id,name,url,spki_fingerprint,identity_state,created_at,updated_at "
        "FROM beacons ORDER BY name COLLATE NOCASE,id"), m_Db);
    while (query.next()) {
        rows.append(QVariantMap{
            {QStringLiteral("id"), query.value(0)},
            {QStringLiteral("name"), query.value(1)},
            {QStringLiteral("url"), query.value(2)},
            {QStringLiteral("spkiFingerprint"), query.value(3)},
            {QStringLiteral("identityState"), query.value(4)},
            {QStringLiteral("createdAt"), query.value(5)},
            {QStringLiteral("updatedAt"), query.value(6)},
        });
    }
    return rows;
}

QVariantMap SettingsDatabase::beacon(const QString& beaconId) const
{
    if (!m_Db.isOpen() || beaconId.isEmpty()) return {};
    QSqlQuery query(m_Db);
    query.prepare(QStringLiteral(
        "SELECT id,name,url,spki_fingerprint,identity_state,created_at,updated_at "
        "FROM beacons WHERE id=?"));
    query.addBindValue(beaconId);
    if (!query.exec() || !query.next()) return {};
    return {
        {QStringLiteral("id"), query.value(0)},
        {QStringLiteral("name"), query.value(1)},
        {QStringLiteral("url"), query.value(2)},
        {QStringLiteral("spkiFingerprint"), query.value(3)},
        {QStringLiteral("identityState"), query.value(4)},
        {QStringLiteral("createdAt"), query.value(5)},
        {QStringLiteral("updatedAt"), query.value(6)},
    };
}

bool SettingsDatabase::savePairedBeacon(
        const QString& beaconId,
        const QString& name,
        const QString& url,
        const QString& spkiFingerprint)
{
    if (!m_Db.isOpen() || beaconId.isEmpty() || name.isEmpty()
            || url.isEmpty() || spkiFingerprint.isEmpty()) {
        return false;
    }
    const QString now =
        QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    QSqlQuery query(m_Db);
    query.prepare(QStringLiteral(
        "INSERT INTO beacons "
        "(id,name,url,spki_fingerprint,identity_state,created_at,updated_at) "
        "VALUES (?,?,?,?,'trusted',?,?) "
        "ON CONFLICT(id) DO UPDATE SET "
        "name=excluded.name,url=excluded.url,"
        "spki_fingerprint=excluded.spki_fingerprint,"
        "identity_state='trusted',updated_at=excluded.updated_at"));
    query.addBindValue(beaconId);
    query.addBindValue(name);
    query.addBindValue(url);
    query.addBindValue(spkiFingerprint);
    query.addBindValue(now);
    query.addBindValue(now);
    return query.exec();
}

bool SettingsDatabase::markBeaconIdentityChanged(const QString& beaconId)
{
    if (!m_Db.isOpen() || beaconId.isEmpty()) return false;
    QSqlQuery query(m_Db);
    query.prepare(QStringLiteral(
        "UPDATE beacons SET identity_state='changed',updated_at=? WHERE id=?"));
    query.addBindValue(
        QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    query.addBindValue(beaconId);
    return query.exec() && query.numRowsAffected() == 1;
}

bool SettingsDatabase::removeBeacon(const QString& beaconId)
{
    if (!m_Db.isOpen() || beaconId.isEmpty()) return false;
    QSqlQuery query(m_Db);
    query.prepare(QStringLiteral("DELETE FROM beacons WHERE id=?"));
    query.addBindValue(beaconId);
    return query.exec();
}

QVariantMap SettingsDatabase::wakeRoute(const QString& hostId) const
{
    QVariantMap route{
        {QStringLiteral("hostId"), hostId},
        {QStringLiteral("provider"), QStringLiteral("direct")},
    };
    if (!m_Db.isOpen() || hostId.isEmpty()) return route;
    QSqlQuery query(m_Db);
    query.prepare(QStringLiteral(
        "SELECT provider,beacon_id,beacon_host_id,updated_at "
        "FROM wake_routes WHERE host_id=?"));
    query.addBindValue(hostId);
    if (query.exec() && query.next()) {
        route.insert(QStringLiteral("provider"), query.value(0));
        route.insert(QStringLiteral("beaconId"), query.value(1));
        route.insert(QStringLiteral("beaconHostId"), query.value(2));
        route.insert(QStringLiteral("updatedAt"), query.value(3));
    }
    return route;
}

QVariantList SettingsDatabase::wakeRoutes() const
{
    QVariantList routes;
    if (!m_Db.isOpen()) return routes;
    QSqlQuery query(QStringLiteral(
        "SELECT host_id,provider,beacon_id,beacon_host_id,updated_at "
        "FROM wake_routes ORDER BY host_id"), m_Db);
    while (query.next()) {
        routes.append(QVariantMap{
            {QStringLiteral("hostId"), query.value(0)},
            {QStringLiteral("provider"), query.value(1)},
            {QStringLiteral("beaconId"), query.value(2)},
            {QStringLiteral("beaconHostId"), query.value(3)},
            {QStringLiteral("updatedAt"), query.value(4)},
        });
    }
    return routes;
}

bool SettingsDatabase::setWakeRoute(
        const QString& hostId,
        const QString& provider,
        const QString& beaconId,
        const QString& beaconHostId)
{
    if (!m_Db.isOpen() || hostId.isEmpty()
            || (provider != QLatin1String("direct")
                && provider != QLatin1String("beacon"))) {
        return false;
    }
    if (provider == QLatin1String("beacon")
            && (beaconId.isEmpty() || beaconHostId.isEmpty())) {
        return false;
    }
    QSqlQuery query(m_Db);
    query.prepare(QStringLiteral(
        "INSERT INTO wake_routes "
        "(host_id,provider,beacon_id,beacon_host_id,updated_at) "
        "VALUES (?,?,?,?,?) ON CONFLICT(host_id) DO UPDATE SET "
        "provider=excluded.provider,beacon_id=excluded.beacon_id,"
        "beacon_host_id=excluded.beacon_host_id,"
        "updated_at=excluded.updated_at"));
    query.addBindValue(hostId);
    query.addBindValue(provider);
    query.addBindValue(provider == QLatin1String("beacon")
                           ? QVariant(beaconId) : QVariant());
    query.addBindValue(provider == QLatin1String("beacon")
                           ? QVariant(beaconHostId) : QVariant());
    query.addBindValue(
        QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    return query.exec();
}

int SettingsDatabase::historyRetentionDays() const
{
    return qBound(0, setting(
        QStringLiteral("history.retention_days"), 90).toInt(), 3650);
}

bool SettingsDatabase::setHistoryRetentionDays(int days)
{
    if (!m_Db.isOpen()) return false;
    days = qBound(0, days, 3650);
    if (!m_Db.transaction()) return false;
    QSqlQuery settingQuery(m_Db);
    settingQuery.prepare(QStringLiteral(
        "INSERT INTO settings(key,value) VALUES ('history.retention_days',?) "
        "ON CONFLICT(key) DO UPDATE SET value=excluded.value"));
    settingQuery.addBindValue(days);
    if (!settingQuery.exec()) {
        m_Db.rollback();
        return false;
    }
    QSqlQuery prune(m_Db);
    if (days == 0) {
        prune.prepare(QStringLiteral("DELETE FROM local_history"));
    } else {
        prune.prepare(QStringLiteral(
            "DELETE FROM local_history WHERE ts < ?"));
        prune.addBindValue(
            QDateTime::currentDateTimeUtc().addDays(-days)
                .toString(Qt::ISODateWithMs));
    }
    if (!prune.exec() || !m_Db.commit()) {
        m_Db.rollback();
        return false;
    }
    return true;
}

bool SettingsDatabase::clearLocalHistory()
{
    if (!m_Db.isOpen()) return false;
    QSqlQuery query(QStringLiteral("DELETE FROM local_history"), m_Db);
    return query.exec();
}

bool SettingsDatabase::importLegacyControllerMaps(const QVariantMap& profiles,
                                                  const QString& markerKey)
{
    if (!m_Db.isOpen()) return false;
    if (setting(markerKey, false).toBool()) return true;
    if (!m_Db.transaction()) return false;

    QSqlQuery mapQuery(m_Db);
    mapQuery.prepare(QStringLiteral(
        "INSERT INTO controller_maps "
        "(controller_id,scope,context_key,map_json,updated_at) VALUES (?,?,?,?,?) "
        "ON CONFLICT(controller_id,scope,context_key) DO NOTHING"));
    const QString updatedAt =
        QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    for (auto it = profiles.constBegin(); it != profiles.constEnd(); ++it) {
        QString controllerId;
        QString scope;
        QString contextKey = QStringLiteral("");
        if (it.key().startsWith(QStringLiteral("controller:"))) {
            controllerId = it.key().mid(11);
            scope = QStringLiteral("controller");
        } else if (it.key().startsWith(QStringLiteral("game:"))) {
            const QString legacyKey = it.key().mid(5);
            const qsizetype separator = legacyKey.lastIndexOf(QLatin1Char('|'));
            if (separator <= 0) continue;
            contextKey = legacyKey.left(separator);
            controllerId = legacyKey.mid(separator + 1);
            scope = QStringLiteral("host_application");
        } else {
            continue;
        }
        mapQuery.bindValue(0, controllerId);
        mapQuery.bindValue(1, scope);
        mapQuery.bindValue(2, contextKey);
        mapQuery.bindValue(3, mapToJson(it.value().toMap()));
        mapQuery.bindValue(4, updatedAt);
        if (!mapQuery.exec()) {
            m_Db.rollback();
            setLastError(QStringLiteral("Failed to import Controller Map: %1")
                         .arg(mapQuery.lastError().text()));
            return false;
        }
    }

    QSqlQuery markerQuery(m_Db);
    markerQuery.prepare(QStringLiteral(
        "INSERT INTO settings (key,value) VALUES (?,1) "
        "ON CONFLICT(key) DO UPDATE SET value=1"));
    markerQuery.addBindValue(markerKey);
    if (!markerQuery.exec() || !m_Db.commit()) {
        m_Db.rollback();
        setLastError(QStringLiteral("Failed to commit Controller Map import"));
        return false;
    }
    setLastError(QString());
    return true;
}

QString SettingsDatabase::defaultDatabasePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return QDir(dir).filePath(QStringLiteral("jochona.db"));
}

int SettingsDatabase::latestKnownSchemaVersion()
{
    const QVector<Migration> all = migrations();
    return all.isEmpty() ? 0 : all.constLast().version;
}

QVector<SettingsDatabase::Migration> SettingsDatabase::migrations()
{
    // hosts.id, collections.id and events.id are surrogate keys assigned by
    // the application (hosts.id is the paired host's certificate UUID,
    // already used as NvComputer::uuid elsewhere in this codebase).
    // host_apps.id is the one column with no natural application-supplied
    // key, so it is the only AUTOINCREMENT column in this schema.
    static const QVector<Migration> kMigrations = {
        {
            1,
            QStringLiteral("Initial schema: settings, hosts, host apps, favorites, collections, and events"),
            {
                QStringLiteral("CREATE TABLE settings ("
                                "key TEXT PRIMARY KEY,"
                                "value TEXT"
                                ")"),
                QStringLiteral("CREATE TABLE hosts ("
                                "id TEXT PRIMARY KEY,"
                                "name TEXT NOT NULL,"
                                "last_address TEXT,"
                                "last_port INTEGER,"
                                "mac TEXT,"
                                "manual_mac TEXT,"
                                "manual_port INTEGER,"
                                "broadcast_candidates TEXT,"
                                "fingerprint TEXT,"
                                "paired INTEGER NOT NULL DEFAULT 0,"
                                "sort_order INTEGER NOT NULL DEFAULT 0,"
                                "updated_at TEXT"
                                ")"),
                QStringLiteral("CREATE TABLE host_apps ("
                                "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                                "host_id TEXT NOT NULL REFERENCES hosts(id) ON DELETE CASCADE,"
                                "app_id TEXT NOT NULL,"
                                "name TEXT NOT NULL,"
                                "bundle TEXT,"
                                "kind TEXT NOT NULL DEFAULT 'game',"
                                "artwork_hash TEXT,"
                                "last_played TEXT,"
                                "play_count INTEGER NOT NULL DEFAULT 0"
                                ")"),
                QStringLiteral("CREATE INDEX idx_host_apps_host_id ON host_apps(host_id)"),
                QStringLiteral("CREATE TABLE favorites ("
                                "host_id TEXT NOT NULL REFERENCES hosts(id) ON DELETE CASCADE,"
                                "app_id TEXT NOT NULL,"
                                "PRIMARY KEY (host_id, app_id)"
                                ")"),
                QStringLiteral("CREATE TABLE collections ("
                                "id INTEGER PRIMARY KEY,"
                                "name TEXT NOT NULL,"
                                "sort_order INTEGER NOT NULL DEFAULT 0"
                                ")"),
                QStringLiteral("CREATE TABLE collection_items ("
                                "collection_id INTEGER NOT NULL REFERENCES collections(id) ON DELETE CASCADE,"
                                "host_id TEXT NOT NULL,"
                                "app_id TEXT NOT NULL,"
                                "UNIQUE(collection_id, host_id, app_id)"
                                ")"),
                QStringLiteral("CREATE TABLE events ("
                                "id INTEGER PRIMARY KEY,"
                                "ts TEXT NOT NULL,"
                                "kind TEXT NOT NULL,"
                                "host_id TEXT,"
                                "app_id TEXT,"
                                "payload TEXT"
                                ")"),
                QStringLiteral("CREATE INDEX idx_events_host_id ON events(host_id)"),
            },
        },
        {
            2,
            QStringLiteral("Milestone 2 settings, library, controller, and capability model"),
            {
                QStringLiteral("ALTER TABLE hosts ADD COLUMN identity_state TEXT "
                               "NOT NULL DEFAULT 'trusted'"),
                QStringLiteral("CREATE TABLE client_devices ("
                               "id TEXT PRIMARY KEY,"
                               "name TEXT NOT NULL,"
                               "created_at TEXT NOT NULL"
                               ")"),
                QStringLiteral("CREATE TABLE streaming_profiles ("
                               "id TEXT PRIMARY KEY,"
                               "name TEXT NOT NULL UNIQUE,"
                               "values_json TEXT NOT NULL,"
                               "created_at TEXT NOT NULL,"
                               "updated_at TEXT NOT NULL"
                               ")"),
                QStringLiteral("CREATE TABLE display_contexts ("
                               "id TEXT PRIMARY KEY,"
                               "client_device_id TEXT NOT NULL REFERENCES client_devices(id) ON DELETE CASCADE,"
                               "name TEXT NOT NULL,"
                               "fingerprint TEXT NOT NULL,"
                               "dock_state TEXT NOT NULL,"
                               "streaming_profile_id TEXT REFERENCES streaming_profiles(id),"
                               "UNIQUE(client_device_id, fingerprint, dock_state)"
                               ")"),
                QStringLiteral("CREATE TABLE settings_patches ("
                               "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                               "scope TEXT NOT NULL CHECK(scope IN "
                               "('host_client_pair','library_entry','host_application')),"
                               "context_key TEXT NOT NULL,"
                               "values_json TEXT NOT NULL DEFAULT '{}',"
                               "pins_json TEXT NOT NULL DEFAULT '{}',"
                               "floors_json TEXT NOT NULL DEFAULT '{}',"
                               "updated_at TEXT NOT NULL,"
                               "UNIQUE(scope, context_key)"
                               ")"),
                QStringLiteral("CREATE TABLE library_entries ("
                               "id TEXT PRIMARY KEY,"
                               "title TEXT NOT NULL,"
                               "kind TEXT NOT NULL DEFAULT 'game',"
                               "artwork_path TEXT,"
                               "favorite INTEGER NOT NULL DEFAULT 0,"
                               "hidden INTEGER NOT NULL DEFAULT 0,"
                               "created_at TEXT NOT NULL,"
                               "updated_at TEXT NOT NULL"
                               ")"),
                QStringLiteral("CREATE TABLE library_entry_apps ("
                               "library_entry_id TEXT NOT NULL REFERENCES library_entries(id) ON DELETE CASCADE,"
                               "host_app_id INTEGER NOT NULL REFERENCES host_apps(id) ON DELETE CASCADE,"
                               "metadata_json TEXT NOT NULL DEFAULT '{}',"
                               "PRIMARY KEY(library_entry_id, host_app_id)"
                               ")"),
                QStringLiteral("CREATE INDEX idx_library_entry_apps_host_app "
                               "ON library_entry_apps(host_app_id)"),
                QStringLiteral("CREATE TABLE host_choice_pins ("
                               "library_entry_id TEXT PRIMARY KEY REFERENCES library_entries(id) ON DELETE CASCADE,"
                               "host_id TEXT NOT NULL REFERENCES hosts(id) ON DELETE CASCADE"
                               ")"),
                QStringLiteral("CREATE TABLE controller_maps ("
                               "controller_id TEXT NOT NULL,"
                               "scope TEXT NOT NULL CHECK(scope IN "
                               "('controller','library_entry','host_application')),"
                               "context_key TEXT NOT NULL DEFAULT '',"
                               "map_json TEXT NOT NULL,"
                               "updated_at TEXT NOT NULL,"
                               "PRIMARY KEY(controller_id, scope, context_key)"
                               ")"),
                QStringLiteral("CREATE TABLE capability_cache ("
                               "host_id TEXT PRIMARY KEY REFERENCES hosts(id) ON DELETE CASCADE,"
                               "capabilities_json TEXT NOT NULL,"
                               "confidence TEXT NOT NULL,"
                               "verified_at TEXT NOT NULL"
                               ")"),
                QStringLiteral("CREATE TABLE local_history ("
                               "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                               "ts TEXT NOT NULL,"
                               "kind TEXT NOT NULL,"
                               "host_id TEXT,"
                               "library_entry_id TEXT,"
                               "host_app_id INTEGER,"
                               "summary_json TEXT NOT NULL DEFAULT '{}'"
                               ")"),
                QStringLiteral("CREATE INDEX idx_local_history_ts "
                               "ON local_history(ts)"),
            },
        },
        {
            3,
            QStringLiteral("Library Host Application identity index"),
            {
                QStringLiteral("CREATE UNIQUE INDEX idx_host_apps_identity "
                               "ON host_apps(host_id, app_id)"),
            },
        },
        {
            4,
            QStringLiteral("Client device and display context settings"),
            {
                QStringLiteral("ALTER TABLE client_devices ADD COLUMN "
                               "values_json TEXT NOT NULL DEFAULT '{}'"),
                QStringLiteral("ALTER TABLE client_devices ADD COLUMN "
                               "pins_json TEXT NOT NULL DEFAULT '{}'"),
                QStringLiteral("ALTER TABLE client_devices ADD COLUMN "
                               "floors_json TEXT NOT NULL DEFAULT '{}'"),
                QStringLiteral("ALTER TABLE display_contexts ADD COLUMN "
                               "values_json TEXT NOT NULL DEFAULT '{}'"),
                QStringLiteral("ALTER TABLE display_contexts ADD COLUMN "
                               "pins_json TEXT NOT NULL DEFAULT '{}'"),
                QStringLiteral("ALTER TABLE display_contexts ADD COLUMN "
                               "floors_json TEXT NOT NULL DEFAULT '{}'"),
                QStringLiteral("ALTER TABLE display_contexts ADD COLUMN "
                               "width INTEGER NOT NULL DEFAULT 0"),
                QStringLiteral("ALTER TABLE display_contexts ADD COLUMN "
                               "height INTEGER NOT NULL DEFAULT 0"),
                QStringLiteral("ALTER TABLE display_contexts ADD COLUMN "
                               "refresh_hz REAL NOT NULL DEFAULT 0"),
                QStringLiteral("ALTER TABLE display_contexts ADD COLUMN "
                               "hdr_capable INTEGER NOT NULL DEFAULT 0"),
                QStringLiteral("ALTER TABLE display_contexts ADD COLUMN "
                               "last_seen TEXT"),
            },
        },
        {
            5,
            QStringLiteral("Paired Beacons and explicit per-Host Wake routes"),
            {
                QStringLiteral("CREATE TABLE beacons ("
                               "id TEXT PRIMARY KEY,"
                               "name TEXT NOT NULL,"
                               "url TEXT NOT NULL,"
                               "spki_fingerprint TEXT NOT NULL,"
                               "identity_state TEXT NOT NULL DEFAULT 'trusted' "
                               "CHECK(identity_state IN ('trusted','changed')),"
                               "created_at TEXT NOT NULL,"
                               "updated_at TEXT NOT NULL"
                               ")"),
                QStringLiteral("CREATE TABLE wake_routes ("
                               "host_id TEXT PRIMARY KEY,"
                               "provider TEXT NOT NULL "
                               "CHECK(provider IN ('direct','beacon')),"
                               "beacon_id TEXT REFERENCES beacons(id) "
                               "ON DELETE CASCADE,"
                               "beacon_host_id TEXT,"
                               "updated_at TEXT NOT NULL,"
                               "CHECK((provider='direct' AND beacon_id IS NULL "
                               "AND beacon_host_id IS NULL) OR "
                               "(provider='beacon' AND beacon_id IS NOT NULL "
                               "AND beacon_host_id IS NOT NULL))"
                               ")"),
                QStringLiteral("CREATE INDEX idx_wake_routes_beacon "
                               "ON wake_routes(beacon_id)"),
            },
        },
    };
    return kMigrations;
}
