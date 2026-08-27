#include "settingsdatabase.h"

#include <QAtomicInteger>
#include <QDateTime>
#include <QDir>
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

SettingsDatabase::SettingsDatabase(QObject* parent)
    : QObject(parent)
{
}

SettingsDatabase::~SettingsDatabase()
{
    closeConnection();
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
    };
    return kMigrations;
}
