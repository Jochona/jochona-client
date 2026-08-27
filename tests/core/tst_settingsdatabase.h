#pragma once

#include <QObject>

class TestSettingsDatabase : public QObject
{
    Q_OBJECT

private slots:
    void freshInstallCreatesLatestSchema();
    void reopenIsIdempotent();
    void backupSnapshotIsCreatedBeforeMigration();
    void refusesSchemaNewerThanKnown();
    void settingRoundTrip();
};
