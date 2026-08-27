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
