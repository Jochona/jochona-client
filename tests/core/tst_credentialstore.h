#pragma once

#include <QObject>
#include <QString>

class TestCredentialStore : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void secretRoundTrip();
    void missingSecretReturnsEmpty();
    void removedSecretIsGone();
    void listAccountsReflectsStoredSecrets();

private:
    QString m_Service;
};
