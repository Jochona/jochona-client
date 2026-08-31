#include "tst_settingsdatabase.h"

#include "core/settingsdatabase.h"

#include <QFile>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

void TestSettingsDatabase::freshInstallCreatesLatestSchema()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("jochona.db"));

    SettingsDatabase db;
    QVERIFY2(db.open(path), qPrintable(db.lastError()));
    QVERIFY(db.isOpen());
    QCOMPARE(db.schemaVersion(), SettingsDatabase::latestKnownSchemaVersion());
    QCOMPARE(db.databasePath(), path);

    {
        QSqlDatabase raw = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("fresh-install-check"));
        raw.setDatabaseName(path);
        QVERIFY(raw.open());

        QSqlQuery query(raw);
        QVERIFY(query.exec(QStringLiteral("SELECT name FROM sqlite_master WHERE type='table'")));
        QSet<QString> tables;
        while (query.next()) {
            tables.insert(query.value(0).toString());
        }

        static const char* kExpectedTables[] = {
            "migrations", "settings", "hosts", "host_apps", "favorites",
            "collections", "collection_items", "events", "client_devices",
            "streaming_profiles", "display_contexts", "settings_patches",
            "library_entries", "library_entry_apps", "host_choice_pins",
            "controller_maps", "capability_cache", "local_history",
            "beacons", "wake_routes"
        };
        for (const char* expected : kExpectedTables) {
            QVERIFY2(tables.contains(QString::fromLatin1(expected)), expected);
        }
    }
    QSqlDatabase::removeDatabase(QStringLiteral("fresh-install-check"));
}

void TestSettingsDatabase::reopenIsIdempotent()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("jochona.db"));

    {
        SettingsDatabase db;
        QVERIFY2(db.open(path), qPrintable(db.lastError()));
    }

    SettingsDatabase db2;
    QVERIFY2(db2.open(path), qPrintable(db2.lastError()));
    QCOMPARE(db2.schemaVersion(), SettingsDatabase::latestKnownSchemaVersion());

    {
        QSqlDatabase raw = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("idempotent-check"));
        raw.setDatabaseName(path);
        QVERIFY(raw.open());

        QSqlQuery query(raw);
        QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM migrations WHERE version = 1")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 1);
    }
    QSqlDatabase::removeDatabase(QStringLiteral("idempotent-check"));
    // The no-op re-run must not create a snapshot for a migration newer
    // than this build.
    QVERIFY(!QFile::exists(
        path + QStringLiteral(".bak-%1")
                   .arg(SettingsDatabase::latestKnownSchemaVersion() + 1)));
}

void TestSettingsDatabase::backupSnapshotIsCreatedBeforeMigration()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("jochona.db"));

    SettingsDatabase db;
    QVERIFY2(db.open(path), qPrintable(db.lastError()));

    // Bounded pruning (see boundedSnapshotPruningKeepsRecentBackups) only
    // guarantees the most recently applied migration's snapshot survives;
    // that is always the one every open() actually needs to roll back to.
    QVERIFY(QFile::exists(
        path + QStringLiteral(".bak-%1").arg(SettingsDatabase::latestKnownSchemaVersion())));
}

void TestSettingsDatabase::refusesSchemaNewerThanKnown()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("jochona.db"));

    {
        SettingsDatabase db;
        QVERIFY2(db.open(path), qPrintable(db.lastError()));
    }

    const int futureVersion = SettingsDatabase::latestKnownSchemaVersion() + 1;
    {
        QSqlDatabase raw = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("future-schema-inject"));
        raw.setDatabaseName(path);
        QVERIFY(raw.open());

        QSqlQuery query(raw);
        query.prepare(QStringLiteral("INSERT INTO migrations (version, applied_at) VALUES (?, ?)"));
        query.addBindValue(futureVersion);
        query.addBindValue(QStringLiteral("2099-01-01T00:00:00Z"));
        QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
    }
    QSqlDatabase::removeDatabase(QStringLiteral("future-schema-inject"));

    SettingsDatabase db2;
    QVERIFY(!db2.open(path));
    QVERIFY(!db2.isOpen());
    QVERIFY(!db2.lastError().isEmpty());

    // Refusing to open must not have touched the on-disk schema version.
    {
        QSqlDatabase raw = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("future-schema-verify"));
        raw.setDatabaseName(path);
        QVERIFY(raw.open());

        QSqlQuery query(raw);
        QVERIFY(query.exec(QStringLiteral("SELECT MAX(version) FROM migrations")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), futureVersion);
    }
    QSqlDatabase::removeDatabase(QStringLiteral("future-schema-verify"));
}

void TestSettingsDatabase::settingRoundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("jochona.db"));

    SettingsDatabase db;
    QVERIFY2(db.open(path), qPrintable(db.lastError()));

    QCOMPARE(db.setting(QStringLiteral("missing"), QStringLiteral("fallback")).toString(), QStringLiteral("fallback"));

    db.setSetting(QStringLiteral("theme"), QStringLiteral("dark"));
    QCOMPARE(db.setting(QStringLiteral("theme")).toString(), QStringLiteral("dark"));

    db.setSetting(QStringLiteral("theme"), QStringLiteral("light"));
    QCOMPARE(db.setting(QStringLiteral("theme")).toString(), QStringLiteral("light"));
}

void TestSettingsDatabase::milestoneTwoRepositoriesRoundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SettingsDatabase db;
    QVERIFY2(db.open(dir.filePath(QStringLiteral("jochona.db"))),
             qPrintable(db.lastError()));

    const QString deviceId = QStringLiteral("device-1");
    QVERIFY(db.ensureClientDevice(deviceId, QStringLiteral("Living Room")));
    const QVariantMap deviceValues{{QStringLiteral("bitrateKbps"), 30000}};
    const QVariantMap devicePins{{QStringLiteral("bitrateKbps"), true}};
    const QVariantMap deviceFloors{{QStringLiteral("fps"), 60}};
    QVERIFY(db.setClientDeviceSettings(deviceId, deviceValues,
                                       devicePins, deviceFloors));
    const QVariantMap device = db.clientDeviceSettings(deviceId);
    QCOMPARE(device.value(QStringLiteral("values")).toMap(), deviceValues);
    QCOMPARE(device.value(QStringLiteral("pins")).toMap(), devicePins);
    QCOMPARE(device.value(QStringLiteral("floors")).toMap(), deviceFloors);

    const QString profileId = QStringLiteral("profile-1");
    const QVariantMap profileValues{
        {QStringLiteral("width"), 2560},
        {QStringLiteral("height"), 1440},
        {QStringLiteral("fps"), 120},
    };
    QVERIFY(db.saveStreamingProfile(profileId, QStringLiteral("Desk"),
                                    profileValues));
    QCOMPARE(db.streamingProfiles().size(), 1);
    QCOMPARE(db.streamingProfile(profileId)
                 .value(QStringLiteral("values")).toMap(), profileValues);

    const QString displayId = QStringLiteral("display-1");
    const QVariantMap metadata{
        {QStringLiteral("width"), 3456},
        {QStringLiteral("height"), 2234},
        {QStringLiteral("refreshHz"), 120.0},
        {QStringLiteral("hdrCapable"), true},
    };
    QVERIFY(db.upsertDisplayContext(
        displayId, deviceId, QStringLiteral("Built-in"),
        QStringLiteral("fingerprint"), QStringLiteral("single"), metadata));
    const QVariantMap displayValues{{QStringLiteral("hdr"), true}};
    QVERIFY(db.setDisplayContextSettings(
        displayId, displayValues, {}, {}, profileId));
    const QVariantMap display = db.displayContext(displayId);
    QCOMPARE(display.value(QStringLiteral("profileId")).toString(),
             profileId);
    QCOMPARE(display.value(QStringLiteral("values")).toMap(), displayValues);
    QCOMPARE(display.value(QStringLiteral("refreshHz")).toDouble(), 120.0);
    QCOMPARE(display.value(QStringLiteral("hdrCapable")).toBool(), true);

    const QVariantMap controllerWide{
        {QStringLiteral("calibration"),
         QVariantMap{{QStringLiteral("deadzoneLeftStick"), 0.15}}},
    };
    const QVariantMap gameMap{
        {QStringLiteral("buttonRemap"),
         QVariantMap{{QStringLiteral("a"), QStringLiteral("b")}}},
    };
    QVERIFY(db.setControllerMap(QStringLiteral("pad-1"),
                                QStringLiteral("controller"), {},
                                controllerWide));
    QVERIFY(db.setControllerMap(QStringLiteral("pad-1"),
                                QStringLiteral("host_application"),
                                QStringLiteral("host|7"), gameMap));
    QCOMPARE(db.controllerMap(QStringLiteral("pad-1"),
                              QStringLiteral("controller"), {}),
             controllerWide);
    QCOMPARE(db.controllerMap(QStringLiteral("pad-1"),
                              QStringLiteral("host_application"),
                              QStringLiteral("host|7")), gameMap);
    QCOMPARE(db.knownControllerIds(), QStringList{QStringLiteral("pad-1")});

    const QVariantMap capabilities{
        {QStringLiteral("family"), QStringLiteral("vibepollo")},
        {QStringLiteral("capabilities"),
         QStringList{QStringLiteral("runtimeBitrate")}},
    };
    const QDateTime verified =
        QDateTime::fromString(QStringLiteral("2026-08-28T00:00:00.000Z"),
                              Qt::ISODateWithMs);
    QVERIFY(db.setCapability(QStringLiteral("host-1"), capabilities,
                             QStringLiteral("confirmed"), verified));
    const QVariantMap cached =
        db.capabilityCache().value(QStringLiteral("host-1")).toMap();
    QCOMPARE(cached.value(QStringLiteral("family")).toString(),
             QStringLiteral("vibepollo"));
    QCOMPARE(cached.value(QStringLiteral("confidence")).toString(),
             QStringLiteral("confirmed"));

    QVERIFY(db.deleteStreamingProfile(profileId));
    QVERIFY(db.streamingProfile(profileId).isEmpty());
    QVERIFY(db.displayContext(displayId)
                .value(QStringLiteral("profileId")).toString().isEmpty());
}

void TestSettingsDatabase::legacyImportsAreOneTimeTransactions()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SettingsDatabase db;
    QVERIFY2(db.open(dir.filePath(QStringLiteral("jochona.db"))),
             qPrintable(db.lastError()));

    const QVariantMap firstController{
        {QStringLiteral("controller:pad"),
         QVariantMap{{QStringLiteral("calibration"),
                      QVariantMap{{QStringLiteral("deadzoneLeftStick"),
                                   0.20}}}}},
        {QStringLiteral("game:42|pad"),
         QVariantMap{{QStringLiteral("buttonRemap"),
                      QVariantMap{{QStringLiteral("a"),
                                   QStringLiteral("b")}}}}},
    };
    const QString controllerMarker =
        QStringLiteral("test.controller_import");
    QVERIFY(db.importLegacyControllerMaps(firstController,
                                          controllerMarker));
    const QVariantMap importedController =
        db.controllerMap(QStringLiteral("pad"),
                         QStringLiteral("controller"), {});
    QCOMPARE(importedController.value(QStringLiteral("calibration"))
                 .toMap().value(QStringLiteral("deadzoneLeftStick"))
                 .toDouble(), 0.20);
    QVERIFY(db.importLegacyControllerMaps(
        {{QStringLiteral("controller:pad"),
          QVariantMap{{QStringLiteral("calibration"),
                       QVariantMap{{QStringLiteral("deadzoneLeftStick"),
                                    0.40}}}}}},
        controllerMarker));
    QCOMPARE(db.controllerMap(QStringLiteral("pad"),
                              QStringLiteral("controller"), {}),
             importedController);

    const QString capabilityMarker =
        QStringLiteral("test.capability_import");
    const QVariantMap firstCapability{
        {QStringLiteral("host-a"),
         QVariantMap{{QStringLiteral("family"),
                      QStringLiteral("apollo")},
                     {QStringLiteral("confidence"),
                      QStringLiteral("confirmed")}}},
    };
    QVERIFY(db.importLegacyCapabilities(firstCapability,
                                        capabilityMarker));
    QVERIFY(db.importLegacyCapabilities(
        {{QStringLiteral("host-a"),
          QVariantMap{{QStringLiteral("family"),
                       QStringLiteral("sunshine")}}}},
        capabilityMarker));
    QCOMPARE(db.capabilityCache()
                 .value(QStringLiteral("host-a")).toMap()
                 .value(QStringLiteral("family")).toString(),
             QStringLiteral("apollo"));
}

void TestSettingsDatabase::beaconRepositoryEnforcesIdentityAndRoutes()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SettingsDatabase db;
    QVERIFY2(db.open(dir.filePath(QStringLiteral("jochona.db"))),
             qPrintable(db.lastError()));

    const QString beaconId =
        QStringLiteral("0f9e1a2b-3c4d-4e5f-8a9b-0c1d2e3f4a5b");
    QVERIFY(db.savePairedBeacon(
        beaconId,
        QStringLiteral("Hallway Beacon"),
        QStringLiteral("https://beacon.local:47100"),
        QStringLiteral("sha256:first")));
    QCOMPARE(db.beacons().size(), 1);
    QCOMPARE(db.beacon(beaconId)
                 .value(QStringLiteral("identityState")).toString(),
             QStringLiteral("trusted"));

    QVERIFY(db.markBeaconIdentityChanged(beaconId));
    QCOMPARE(db.beacon(beaconId)
                 .value(QStringLiteral("identityState")).toString(),
             QStringLiteral("changed"));

    // Only a successful explicit pairing operation may replace the pin and
    // restore trust.
    QVERIFY(db.savePairedBeacon(
        beaconId,
        QStringLiteral("Hallway Beacon"),
        QStringLiteral("https://beacon.local:47100"),
        QStringLiteral("sha256:replacement")));
    const QVariantMap repaired = db.beacon(beaconId);
    QCOMPARE(repaired.value(QStringLiteral("identityState")).toString(),
             QStringLiteral("trusted"));
    QCOMPARE(repaired.value(QStringLiteral("spkiFingerprint")).toString(),
             QStringLiteral("sha256:replacement"));

    const QString hostId = QStringLiteral("host-1");
    QCOMPARE(db.wakeRoute(hostId)
                 .value(QStringLiteral("provider")).toString(),
             QStringLiteral("direct"));
    QVERIFY(!db.setWakeRoute(hostId, QStringLiteral("beacon")));
    QVERIFY(db.setWakeRoute(
        hostId, QStringLiteral("beacon"), beaconId,
        QStringLiteral("beacon-host-1")));
    const QVariantMap route = db.wakeRoute(hostId);
    QCOMPARE(route.value(QStringLiteral("provider")).toString(),
             QStringLiteral("beacon"));
    QCOMPARE(route.value(QStringLiteral("beaconId")).toString(), beaconId);
    QCOMPARE(route.value(QStringLiteral("beaconHostId")).toString(),
             QStringLiteral("beacon-host-1"));

    QVERIFY(db.removeBeacon(beaconId));
    QCOMPARE(db.wakeRoute(hostId)
                 .value(QStringLiteral("provider")).toString(),
             QStringLiteral("direct"));
}

void TestSettingsDatabase::localHistoryRetentionPrunesRows()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("jochona.db"));
    SettingsDatabase db;
    QVERIFY2(db.open(path), qPrintable(db.lastError()));
    QCOMPARE(db.historyRetentionDays(), 90);

    const QString connectionName = QStringLiteral("history-retention-check");
    {
        QSqlDatabase raw =
            QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                      connectionName);
        raw.setDatabaseName(path);
        QVERIFY(raw.open());
        QSqlQuery insert(raw);
        insert.prepare(QStringLiteral(
            "INSERT INTO local_history(ts,kind,summary_json) VALUES (?,?,?)"));
        insert.addBindValue(
            QDateTime::currentDateTimeUtc().addDays(-120)
                .toString(Qt::ISODateWithMs));
        insert.addBindValue(QStringLiteral("old"));
        insert.addBindValue(QStringLiteral("{}"));
        QVERIFY(insert.exec());
        insert.bindValue(0, QDateTime::currentDateTimeUtc()
                                .toString(Qt::ISODateWithMs));
        insert.bindValue(1, QStringLiteral("recent"));
        insert.bindValue(2, QStringLiteral("{}"));
        QVERIFY(insert.exec());

        QVERIFY(db.setHistoryRetentionDays(90));
        QSqlQuery count(raw);
        QVERIFY(count.exec(QStringLiteral(
            "SELECT kind FROM local_history ORDER BY kind")));
        QVERIFY(count.next());
        QCOMPARE(count.value(0).toString(), QStringLiteral("recent"));
        QVERIFY(!count.next());

        QVERIFY(db.setHistoryRetentionDays(0));
        QVERIFY(count.exec(QStringLiteral(
            "SELECT COUNT(*) FROM local_history")));
        QVERIFY(count.next());
        QCOMPARE(count.value(0).toInt(), 0);
        QCOMPARE(db.historyRetentionDays(), 0);
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void TestSettingsDatabase::hostRecordsSurviveAsExactRecords()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("jochona.db"));

    const QVariantMap record{
        {QStringLiteral("uuid"), QStringLiteral("host-uuid-1")},
        {QStringLiteral("name"), QStringLiteral("Living Room PC")},
        {QStringLiteral("hasCustomName"), true},
        {QStringLiteral("mac"), QStringLiteral("AABBCCDDEEFF")},
        {QStringLiteral("localAddress"), QStringLiteral("192.168.1.50")},
        {QStringLiteral("localPort"), 47989},
        {QStringLiteral("serverCert"), QStringLiteral("-----BEGIN CERTIFICATE-----\nfake\n-----END CERTIFICATE-----\n")},
        {QStringLiteral("identityChanged"), true},
        {QStringLiteral("pendingServerCert"), QStringLiteral("-----BEGIN CERTIFICATE-----\npending\n-----END CERTIFICATE-----\n")},
        {QStringLiteral("apps"), QVariantList{
            QVariantMap{{QStringLiteral("id"), 1234},
                       {QStringLiteral("name"), QStringLiteral("Some Game")},
                       {QStringLiteral("hdrSupported"), true}},
        }},
    };

    {
        SettingsDatabase db;
        QVERIFY2(db.open(path), qPrintable(db.lastError()));
        QVERIFY(db.replaceHostRecords(QVariantList{record}));
    }

    // Paired Hosts must survive a restart from SQLite alone.
    SettingsDatabase reopened;
    QVERIFY2(reopened.open(path), qPrintable(reopened.lastError()));
    const QVariantList records = reopened.hostRecords();
    QCOMPARE(records.size(), 1);
    const QVariantMap roundTripped = records.first().toMap();
    QCOMPARE(roundTripped, record);

    // The normalized hosts table is a projection: a placeholder row must
    // exist so host_records' foreign key is satisfiable, without this
    // call having invented Host content LibraryManager owns.
    const QString connectionName = QStringLiteral("host-record-projection-check");
    {
        QSqlDatabase raw =
            QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        raw.setDatabaseName(path);
        QVERIFY(raw.open());
        QSqlQuery hostQuery(raw);
        QVERIFY(hostQuery.exec(
            QStringLiteral("SELECT id FROM hosts WHERE id = 'host-uuid-1'")));
        QVERIFY(hostQuery.next());
        raw.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    // A subsequent replace must drop records no longer present, mirroring
    // the in-memory Host set exactly (e.g. after a deleteHost()).
    {
        SettingsDatabase db;
        QVERIFY2(db.open(path), qPrintable(db.lastError()));
        QVERIFY(db.replaceHostRecords(QVariantList{}));
        QVERIFY(db.hostRecords().isEmpty());
    }
}

void TestSettingsDatabase::legacyHostRecordsImportIsOneTimeTransaction()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SettingsDatabase db;
    QVERIFY2(db.open(dir.filePath(QStringLiteral("jochona.db"))),
             qPrintable(db.lastError()));

    const QString marker = QStringLiteral("test.host_record_import");
    const QVariantMap firstRecord{
        {QStringLiteral("uuid"), QStringLiteral("host-legacy")},
        {QStringLiteral("name"), QStringLiteral("Legacy PC")},
        {QStringLiteral("wakePort"), 0},
    };
    QVERIFY(db.importLegacyHostRecords(QVariantList{firstRecord}, marker));
    QCOMPARE(db.hostRecords().size(), 1);
    QCOMPARE(db.hostRecords().first().toMap().value(QStringLiteral("name")).toString(),
             QStringLiteral("Legacy PC"));

    // A second import attempt (e.g. a retried migration) must not touch
    // the already-imported Host set.
    const QVariantMap secondRecord{
        {QStringLiteral("uuid"), QStringLiteral("host-legacy")},
        {QStringLiteral("name"), QStringLiteral("Renamed Elsewhere")},
        {QStringLiteral("wakePort"), 0},
    };
    QVERIFY(db.importLegacyHostRecords(QVariantList{secondRecord}, marker));
    QCOMPARE(db.hostRecords().size(), 1);
    QCOMPARE(db.hostRecords().first().toMap().value(QStringLiteral("name")).toString(),
             QStringLiteral("Legacy PC"));
}

void TestSettingsDatabase::failedMigrationRestoresPriorDatabase()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("jochona.db"));

    const int latest = SettingsDatabase::latestKnownSchemaVersion();

    {
        SettingsDatabase db;
        QVERIFY2(db.open(path), qPrintable(db.lastError()));
        db.setSetting(QStringLiteral("survivesRollback"), QStringLiteral("yes"));
    }

    // Simulate a migration that failed part-way through: rewind the
    // recorded schema version without undoing the schema objects it
    // created, so the next open() reapplies the latest migration and hits
    // a real "already exists" failure from SQLite.
    {
        const QString connectionName = QStringLiteral("rollback-rewind");
        QSqlDatabase raw = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        raw.setDatabaseName(path);
        QVERIFY(raw.open());
        QSqlQuery query(raw);
        query.prepare(QStringLiteral("DELETE FROM migrations WHERE version = ?"));
        query.addBindValue(latest);
        QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
        raw.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("rollback-rewind"));

    {
        SettingsDatabase failing;
        QVERIFY(!failing.open(path));
        QVERIFY(!failing.isOpen());
        QVERIFY(!failing.lastError().isEmpty());
    }

    // The database must still be usable and show the earlier (pre-rewind)
    // recorded schema version, with pre-existing data intact -- the failed
    // migration attempt must not have corrupted it.
    {
        const QString connectionName = QStringLiteral("rollback-verify");
        QSqlDatabase raw = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        raw.setDatabaseName(path);
        QVERIFY(raw.open());

        QSqlQuery versionQuery(raw);
        QVERIFY(versionQuery.exec(QStringLiteral("SELECT COALESCE(MAX(version),0) FROM migrations")));
        QVERIFY(versionQuery.next());
        QCOMPARE(versionQuery.value(0).toInt(), latest - 1);

        QSqlQuery settingQuery(raw);
        settingQuery.prepare(QStringLiteral("SELECT value FROM settings WHERE key = ?"));
        settingQuery.addBindValue(QStringLiteral("survivesRollback"));
        QVERIFY(settingQuery.exec());
        QVERIFY(settingQuery.next());
        QCOMPARE(settingQuery.value(0).toString(), QStringLiteral("yes"));
        raw.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("rollback-verify"));
}

void TestSettingsDatabase::boundedSnapshotPruningKeepsRecentBackups()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("jochona.db"));

    SettingsDatabase db;
    QVERIFY2(db.open(path), qPrintable(db.lastError()));

    const int latest = SettingsDatabase::latestKnownSchemaVersion();
    QVERIFY(latest > 3);

    for (int version = 1; version <= latest - 3; version++) {
        QVERIFY(!QFile::exists(path + QStringLiteral(".bak-%1").arg(version)));
    }
    for (int version = latest - 2; version <= latest; version++) {
        QVERIFY(QFile::exists(path + QStringLiteral(".bak-%1").arg(version)));
    }
}
