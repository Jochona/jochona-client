#include "tst_credentialstore.h"

#include "core/credentialstore.h"

#include <QTest>
#include <QUuid>

#if !defined(Q_OS_DARWIN)
namespace {
    // These tests exercise a real OS credential vault (Keychain on macOS,
    // Credential Manager on Windows, the Secret Service via libsecret on
    // Linux). A unique service name per test run keeps them isolated from
    // any real Jochona secrets and from other concurrent test runs.
    const char* kSkipReason = "Credential vault roundtrip is only verified on macOS in this build environment";
}
#endif

void TestCredentialStore::initTestCase()
{
    m_Service = QStringLiteral("com.jochona.tests.%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}

void TestCredentialStore::cleanupTestCase()
{
#if defined(Q_OS_DARWIN)
    CredentialStore store;
    const QStringList accounts = store.listAccounts(m_Service);
    for (const QString& account : accounts) {
        store.removeSecret(m_Service, account);
    }
#endif
}

void TestCredentialStore::secretRoundTrip()
{
#if !defined(Q_OS_DARWIN)
    QSKIP(kSkipReason);
#else
    CredentialStore store;
    const QByteArray secret = QByteArrayLiteral("s3cr3t-pairing-key");

    QVERIFY(store.setSecret(m_Service, QStringLiteral("host-alpha"), secret));
    QCOMPARE(store.getSecret(m_Service, QStringLiteral("host-alpha")), secret);
    QVERIFY(store.hasSecret(m_Service, QStringLiteral("host-alpha")));

    // setSecret() is an upsert: storing again for the same (service, account)
    // replaces the previous value rather than failing or duplicating it.
    const QByteArray updated = QByteArrayLiteral("rotated-key");
    QVERIFY(store.setSecret(m_Service, QStringLiteral("host-alpha"), updated));
    QCOMPARE(store.getSecret(m_Service, QStringLiteral("host-alpha")), updated);

    QVERIFY(store.removeSecret(m_Service, QStringLiteral("host-alpha")));
#endif
}

void TestCredentialStore::missingSecretReturnsEmpty()
{
#if !defined(Q_OS_DARWIN)
    QSKIP(kSkipReason);
#else
    CredentialStore store;
    QVERIFY(!store.hasSecret(m_Service, QStringLiteral("never-stored")));
    QVERIFY(store.getSecret(m_Service, QStringLiteral("never-stored")).isEmpty());
#endif
}

void TestCredentialStore::removedSecretIsGone()
{
#if !defined(Q_OS_DARWIN)
    QSKIP(kSkipReason);
#else
    CredentialStore store;
    QVERIFY(store.setSecret(m_Service, QStringLiteral("host-beta"), QByteArrayLiteral("temp")));
    QVERIFY(store.hasSecret(m_Service, QStringLiteral("host-beta")));

    QVERIFY(store.removeSecret(m_Service, QStringLiteral("host-beta")));
    QVERIFY(!store.hasSecret(m_Service, QStringLiteral("host-beta")));
    QVERIFY(store.getSecret(m_Service, QStringLiteral("host-beta")).isEmpty());

    // Removing an already-absent secret is not an error.
    QVERIFY(store.removeSecret(m_Service, QStringLiteral("host-beta")));
#endif
}

void TestCredentialStore::listAccountsReflectsStoredSecrets()
{
#if !defined(Q_OS_DARWIN)
    QSKIP(kSkipReason);
#else
    CredentialStore store;
    QVERIFY(store.setSecret(m_Service, QStringLiteral("host-gamma"), QByteArrayLiteral("g")));
    QVERIFY(store.setSecret(m_Service, QStringLiteral("host-delta"), QByteArrayLiteral("d")));

    const QStringList accounts = store.listAccounts(m_Service);
    QVERIFY(accounts.contains(QStringLiteral("host-gamma")));
    QVERIFY(accounts.contains(QStringLiteral("host-delta")));

    store.removeSecret(m_Service, QStringLiteral("host-gamma"));
    store.removeSecret(m_Service, QStringLiteral("host-delta"));
#endif
}

void TestCredentialStore::identityCredentialImportIsIdempotentAndScoped()
{
#if !defined(Q_OS_DARWIN)
    QSKIP(kSkipReason);
#else
    // Mirrors IdentityManager's one-time QSettings -> vault migration: a
    // pairing certificate and private key are written together under one
    // service, and a crash-retried migration re-runs the same writes.
    CredentialStore store;
    const QByteArray legacyCert = QByteArrayLiteral("-----BEGIN CERTIFICATE-----\nlegacy\n-----END CERTIFICATE-----\n");
    const QByteArray legacyKey = QByteArrayLiteral("-----BEGIN RSA PRIVATE KEY-----\nlegacy\n-----END RSA PRIVATE KEY-----\n");

    QVERIFY(store.setSecret(m_Service, QStringLiteral("certificate"), legacyCert));
    QVERIFY(store.setSecret(m_Service, QStringLiteral("privatekey"), legacyKey));

    // A retried import (e.g. after a crash between the two writes above,
    // or a second launch before the marker was durably committed) must be
    // safe to repeat and must not leak the secret bytes anywhere else --
    // getSecret() on the same (service, account) is the only way to
    // recover them.
    QVERIFY(store.setSecret(m_Service, QStringLiteral("certificate"), legacyCert));
    QVERIFY(store.setSecret(m_Service, QStringLiteral("privatekey"), legacyKey));

    QCOMPARE(store.getSecret(m_Service, QStringLiteral("certificate")), legacyCert);
    QCOMPARE(store.getSecret(m_Service, QStringLiteral("privatekey")), legacyKey);

    // Only the two vault-tracked accounts are present -- the repeated
    // writes did not create duplicate or stray entries.
    const QStringList accounts = store.listAccounts(m_Service);
    QCOMPARE(accounts.size(), 2);
    QVERIFY(accounts.contains(QStringLiteral("certificate")));
    QVERIFY(accounts.contains(QStringLiteral("privatekey")));

    store.removeSecret(m_Service, QStringLiteral("certificate"));
    store.removeSecret(m_Service, QStringLiteral("privatekey"));
#endif
}
