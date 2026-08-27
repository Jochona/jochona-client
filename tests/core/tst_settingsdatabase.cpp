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
            "migrations", "settings", "hosts", "host_apps", "favorites", "collections", "collection_items", "events"
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

    // The no-op re-run must not have applied migration 1 again, so no
    // second snapshot should exist.
    QVERIFY(!QFile::exists(path + QStringLiteral(".bak-2")));
}

void TestSettingsDatabase::backupSnapshotIsCreatedBeforeMigration()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("jochona.db"));

    SettingsDatabase db;
    QVERIFY2(db.open(path), qPrintable(db.lastError()));

    QVERIFY(QFile::exists(path + QStringLiteral(".bak-1")));
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
