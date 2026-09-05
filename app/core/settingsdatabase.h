#pragma once

#include <QDateTime>
#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>
#include <QVector>
#include <QVariantList>

// SettingsDatabase owns the single SQLite database that stores settings,
// paired Hosts, per-host application caches, favorites, collections, and
// the event history (proposal.md 6.15, 7.1). It never stores secrets --
// see CredentialStore for the OS vault used for pairing keys and tokens.
//
// Paired Hosts: host_records.record_json holds the exact, lossless
// serialization of every persisted NvComputer field (see
// ComputerManager's hostRecordFromComputer()/computerFromHostRecord());
// SQLite is the single durable authority for this state. The normalized
// `hosts` table remains a query-friendly projection (name/address/mac/...)
// maintained by LibraryManager, not a second source of truth -- host_records
// rows reference it only to guarantee a placeholder row exists.
//
// The database is opened with WAL journaling and foreign keys enabled, and
// schema changes are applied by a small numbered migration runner: each
// migration runs inside its own transaction, and a `VACUUM INTO` snapshot
// of the database is taken immediately before that migration is applied.
// If applying the migration fails, the snapshot is restored automatically
// so the database is left at a usable prior schema version instead of a
// possibly half-migrated one; open() still reports failure so the caller
// knows the requested schema version was not reached. Snapshots older than
// the most recent few are pruned after every successful open() so backups
// do not accumulate without bound across app updates. Opening a database
// whose recorded schema version is newer than this build knows about is
// refused outright (no downgrade path), matching the "no soft proceed
// anyway" posture used elsewhere in the client for irreversible state
// changes.
class SettingsDatabase : public QObject
{
    Q_OBJECT

public:
    explicit SettingsDatabase(QObject* parent = nullptr);

    // Jochona: process-wide instance created at startup (main.cpp). Returns
    // nullptr before startup wiring or after teardown; callers must tolerate.
    static SettingsDatabase* get();

    ~SettingsDatabase() override;

    // Opens (creating if necessary) the database at databasePath, or the
    // default per-platform application data location when databasePath is
    // empty, then runs any pending migrations. Returns false when the open
    // fails, when a migration fails, or when the on-disk schema is newer
    // than latestKnownSchemaVersion() -- see lastError() for details. Safe
    // to call again (e.g. with a different path) on an already-open
    // instance; the previous connection is closed first.
    Q_INVOKABLE bool open(const QString& databasePath = QString());

    bool isOpen() const;

    // The schema version currently recorded in the migrations table, or 0
    // if the database is not open.
    int schemaVersion() const;

    QString databasePath() const;

    // Empty when the last operation succeeded.
    QString lastError() const;

    // Convenience accessors for the "settings" key/value table.
    Q_INVOKABLE QVariant setting(const QString& key, const QVariant& defaultValue = QVariant()) const;

    Q_INVOKABLE void setSetting(const QString& key, const QVariant& value);

    // Atomic batch write used by baseline saves and one-time imports.
    bool setSettings(const QVariantMap& values);

    // Imports namespaced legacy values once. The marker is written in the
    // same transaction, so a crash never produces a half-imported baseline.
    bool importLegacySettings(const QVariantMap& values, const QString& markerKey);

    QVariantMap settingsPatch(const QString& scope,
                              const QString& contextKey) const;
    bool setSettingsPatch(const QString& scope,
                          const QString& contextKey,
                          const QVariantMap& values,
                          const QVariantMap& pins = {},
                          const QVariantMap& floors = {});
    QVariantMap streamingProfile(const QString& profileId) const;
    bool saveStreamingProfile(const QString& profileId,
                              const QString& name,
                              const QVariantMap& values);
    QVariantList streamingProfiles() const;
    bool deleteStreamingProfile(const QString& profileId);
    bool ensureClientDevice(const QString& deviceId, const QString& name);
    QVariantMap clientDeviceSettings(const QString& deviceId) const;
    bool setClientDeviceSettings(const QString& deviceId,
                                 const QVariantMap& values,
                                 const QVariantMap& pins = {},
                                 const QVariantMap& floors = {});
    QVariantMap displayContext(const QString& contextId) const;
    bool upsertDisplayContext(const QString& contextId,
                              const QString& deviceId,
                              const QString& name,
                              const QString& fingerprint,
                              const QString& dockState,
                              const QVariantMap& metadata);
    bool setDisplayContextSettings(const QString& contextId,
                                   const QVariantMap& values,
                                   const QVariantMap& pins = {},
                                   const QVariantMap& floors = {},
                                   const QString& profileId = QString());
    QVariantMap controllerMap(const QString& controllerId,
                              const QString& scope,
                              const QString& contextKey) const;
    bool setControllerMap(const QString& controllerId,
                          const QString& scope,
                          const QString& contextKey,
                          const QVariantMap& map);
    bool removeControllerMap(const QString& controllerId,
                             const QString& scope,
                             const QString& contextKey);
    QStringList knownControllerIds() const;
    QVariantMap capabilityCache() const;
    bool setCapability(const QString& hostId,
                       const QVariantMap& capabilities,
                       const QString& confidence,
                       const QDateTime& verifiedAt);
    bool importLegacyCapabilities(const QVariantMap& capabilities,
                                  const QString& markerKey);
    QVariantList beacons() const;
    QVariantMap beacon(const QString& beaconId) const;
    bool savePairedBeacon(const QString& beaconId,
                          const QString& name,
                          const QString& url,
                          const QString& spkiFingerprint);
    bool markBeaconIdentityChanged(const QString& beaconId);
    bool removeBeacon(const QString& beaconId);
    QVariantMap wakeRoute(const QString& hostId) const;
    QVariantList wakeRoutes() const;
    bool setWakeRoute(const QString& hostId,
                      const QString& provider,
                      const QString& beaconId = QString(),
                      const QString& beaconHostId = QString());
    int historyRetentionDays() const;
    bool setHistoryRetentionDays(int days);
    bool clearLocalHistory();
    bool importLegacyControllerMaps(const QVariantMap& profiles,
                                    const QString& markerKey);

    // Paired Host records (proposal.md 6.15): each entry is the exact
    // serialized NvComputer record produced by ComputerManager, decoded
    // from JSON back into a QVariantMap. Order is unspecified.
    QVariantList hostRecords() const;

    // Transactionally replaces the entire Host record set with exactly
    // `records` (each a map produced by ComputerManager, keyed by "uuid"),
    // deleting any existing record not present in the new set. Used for
    // every ordinary save so a torn write always leaves the previously
    // committed Host set intact rather than a partial one.
    bool replaceHostRecords(const QVariantList& records);

    // Imports legacy QSettings-era Host records once. The marker is
    // written in the same transaction, so a crash never produces a
    // half-imported Host set.
    bool importLegacyHostRecords(const QVariantList& records, const QString& markerKey);

    // The default per-platform location for the database file
    // (QStandardPaths::AppDataLocation + "/jochona.db"), creating the
    // containing directory if it does not already exist.
    static QString defaultDatabasePath();

    // The highest schema version this build knows how to migrate to.
    static int latestKnownSchemaVersion();

    // Number of pre-migration snapshots retained on disk; older ones are
    // pruned after a successful open() so backups do not accumulate
    // without bound as the schema evolves across app updates.
    static int maxRetainedBackupSnapshots();

private:
    struct Migration
    {
        int version;
        QString description;
        QStringList statements;
    };

    static QVector<Migration> migrations();

    bool ensureMigrationsTable();
    int currentSchemaVersion() const;
    QString backupSnapshotPath(int version) const;
    bool backupBeforeMigration(int version);
    bool applyMigration(const Migration& migration);
    bool restoreFromSnapshot(int version);
    void pruneBackupSnapshots();
    bool ensureHostPlaceholder(const QString& hostId, const QString& name);
    bool upsertHostRecordsLocked(const QVariantList& records);
    void setLastError(const QString& error);
    void closeConnection();

    QSqlDatabase m_Db;
    QString m_ConnectionName;
    QString m_DatabasePath;
    QString m_LastError;

    static SettingsDatabase* s_Instance;
};
