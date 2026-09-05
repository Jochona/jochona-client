#pragma once

#include "core/credentialstore.h"

#include <QSslConfiguration>
#include <QSslCertificate>
#include <QSslKey>
#include <QSettings>

// The pairing certificate and private key are secrets, persisted only in
// the OS credential vault (CredentialStore) -- never in QSettings. On the
// first run after this migration ships, pre-existing QSettings cert/key
// bytes are written to a durable vault and verified before the migration
// marker is committed. The plaintext legacy keys are then removed; a crash
// before either step simply retries the import. If no durable vault is
// available, IdentityManager fails hard instead of reading plaintext on
// every launch or creating an identity that would rotate after restart.
class IdentityManager
{
public:
    QString
    getUniqueId();

    QByteArray
    getCertificate();

    QByteArray
    getPrivateKey();

    QSslConfiguration
    getSslConfig();

    static
    IdentityManager*
    get();

private:
    IdentityManager();

    QSslCertificate
    getSslCertificate();

    QSslKey
    getSslKey();

    void
    createCredentials();

    // Owns the vault connection for this process; never construct a second
    // instance, since the Linux in-memory fallback store is per-instance
    // and would otherwise silently lose secrets written through another one.
    CredentialStore m_Vault;

    // Initialized in constructor
    QByteArray m_CachedPrivateKey;
    QByteArray m_CachedPemCert;

    // Lazy initialized
    QString m_CachedUniqueId;
    QSslCertificate m_CachedSslCert;
    QSslKey m_CachedSslKey;

    static IdentityManager* s_Im;
};
