#include "identitymanager.h"
#include "utils.h"

#include "core/settingsdatabase.h"

#include <QDebug>

#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <openssl/rand.h>

#define SER_UNIQUEID "uniqueid"
// Legacy QSettings keys, read (never written) only for the one-time import
// into CredentialStore below -- see IdentityManager's class comment.
#define SER_CERT "certificate"
#define SER_KEY "key"

#define VAULT_SERVICE "com.jochona.client.identity"
#define VAULT_ACCOUNT_CERT "certificate"
#define VAULT_ACCOUNT_KEY "privatekey"
#define IDENTITY_MIGRATION_MARKER "migration.qsettings_identity_imported_v1"

IdentityManager* IdentityManager::s_Im = nullptr;

IdentityManager*
IdentityManager::get()
{
    // This will always be called first on the main thread,
    // so it's safe to initialize without locks.
    if (s_Im == nullptr) {
        s_Im = new IdentityManager();
    }

    return s_Im;
}

void IdentityManager::createCredentials()
{
    X509* cert = X509_new();
    THROW_BAD_ALLOC_IF_NULL(cert);

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    EVP_PKEY* pk = EVP_RSA_gen(2048);
    THROW_BAD_ALLOC_IF_NULL(pk);
#else
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
    THROW_BAD_ALLOC_IF_NULL(ctx);

    EVP_PKEY_keygen_init(ctx);
    EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048);

    // pk must be initialized on input
    EVP_PKEY* pk = NULL;
    EVP_PKEY_keygen(ctx, &pk);

    EVP_PKEY_CTX_free(ctx);
    THROW_BAD_ALLOC_IF_NULL(pk);
#endif

    X509_set_version(cert, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(cert), 0);
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    X509_gmtime_adj(X509_get_notBefore(cert), 0);
    X509_gmtime_adj(X509_get_notAfter(cert), 60 * 60 * 24 * 365 * 20); // 20 yrs
#else
    X509_gmtime_adj(X509_getm_notBefore(cert), 0);
    X509_gmtime_adj(X509_getm_notAfter(cert), 60 * 60 * 24 * 365 * 20); // 20 yrs
#endif

    X509_set_pubkey(cert, pk);

    X509_NAME* name = X509_NAME_new();
    THROW_BAD_ALLOC_IF_NULL(name);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                               reinterpret_cast<unsigned char *>(const_cast<char*>("NVIDIA GameStream Client")),
                               -1, -1, 0);
    X509_set_subject_name(cert, name);
    X509_set_issuer_name(cert, name);
    X509_NAME_free(name);

    X509_sign(cert, pk, EVP_sha256());

    BIO* biokey = BIO_new(BIO_s_mem());
    THROW_BAD_ALLOC_IF_NULL(biokey);
    PEM_write_bio_PrivateKey(biokey, pk, NULL, NULL, 0, NULL, NULL);

    BIO* biocert = BIO_new(BIO_s_mem());
    THROW_BAD_ALLOC_IF_NULL(biocert);
    PEM_write_bio_X509(biocert, cert);

    BUF_MEM* mem;
    BIO_get_mem_ptr(biokey, &mem);
    m_CachedPrivateKey = QByteArray(mem->data, (int)mem->length);

    BIO_get_mem_ptr(biocert, &mem);
    m_CachedPemCert = QByteArray(mem->data, (int)mem->length);

    X509_free(cert);
    EVP_PKEY_free(pk);
    BIO_free(biokey);
    BIO_free(biocert);

    // Check that the new keypair is valid before persisting it
    if (getSslCertificate().isNull()) {
        qFatal("Newly generated certificate is unreadable");
    }
    if (getSslKey().isNull()) {
        qFatal("Newly generated private key is unreadable");
    }

    if (!m_Vault.setSecret(VAULT_SERVICE, VAULT_ACCOUNT_CERT, m_CachedPemCert)
            || !m_Vault.setSecret(VAULT_SERVICE, VAULT_ACCOUNT_KEY, m_CachedPrivateKey)) {
        // The vault is expected to be durable here (callers only reach
        // createCredentials() when it is, or when regenerating an
        // unreadable existing identity); a write failure means we cannot
        // safely continue with an identity that would silently rotate on
        // the next launch.
        qFatal("Failed to persist the newly generated pairing identity to %s; refusing to continue "
               "with an identity that could not survive a restart",
               qPrintable(m_Vault.backendName()));
    }

    qInfo() << "Wrote new identity credentials to" << m_Vault.backendName();
}

IdentityManager::IdentityManager()
{
    QSettings settings;

    SettingsDatabase* database = SettingsDatabase::get();
    const bool alreadyImported = database && database->isOpen()
            && database->setting(QStringLiteral(IDENTITY_MIGRATION_MARKER), false).toBool();
    const bool vaultIsDurable = m_Vault.backendName() != QStringLiteral("in-memory fallback");
    bool migrationRecorded = alreadyImported;

    qInfo() << "Loading Client identity from" << m_Vault.backendName();
    m_CachedPemCert = m_Vault.getSecret(VAULT_SERVICE, VAULT_ACCOUNT_CERT);
    m_CachedPrivateKey = m_Vault.getSecret(VAULT_SERVICE, VAULT_ACCOUNT_KEY);
    qInfo() << "Finished loading Client identity from the credential vault";
    bool fromVault = !m_CachedPemCert.isEmpty() && !m_CachedPrivateKey.isEmpty();

    if (!fromVault) {
        // The vault has no identity yet. This is either the one-time
        // migration off QSettings or a missing persistent vault. A durable
        // vault is mandatory: continuing from plaintext every launch would
        // violate the Credential Vault contract, while generating an
        // in-memory identity would silently rotate it after restart.
        const QByteArray legacyCert = settings.value(SER_CERT).toByteArray();
        const QByteArray legacyKey = settings.value(SER_KEY).toByteArray();

        if (!legacyCert.isEmpty() && !legacyKey.isEmpty()) {
            m_CachedPemCert = legacyCert;
            m_CachedPrivateKey = legacyKey;

            if (vaultIsDurable) {
                if (m_Vault.setSecret(VAULT_SERVICE, VAULT_ACCOUNT_CERT, legacyCert)
                        && m_Vault.setSecret(VAULT_SERVICE, VAULT_ACCOUNT_KEY, legacyKey)) {
                    fromVault = true;
                    qInfo() << "Migrated pairing identity credentials from QSettings to" << m_Vault.backendName();
                }
                else {
                    qFatal("Failed to migrate pairing identity credentials into %s; refusing to "
                           "continue with plaintext QSettings credentials",
                           qPrintable(m_Vault.backendName()));
                }
            }
            else {
                qFatal("No persistent OS credential vault is available (%s); install or start "
                       "the platform credential service before launching Jochona",
                       qPrintable(m_Vault.backendName()));
            }
        }
        else if (alreadyImported) {
            // The vault previously held a durably-verified identity (the
            // marker below is only ever set once a durable vault write is
            // confirmed) but now returns nothing, and there is no legacy
            // copy to recover it from. Regenerating here would silently
            // replace every paired Host's pinned trust with a brand new
            // identity, so refuse to start instead.
            qFatal("The OS credential vault lost the previously verified pairing identity and no "
                   "QSettings fallback is available; refusing to generate a new identity");
        }
    }

    // Regenerating a missing or corrupt identity is only safe when the
    // vault can durably hold the replacement; otherwise it would just
    // rotate again on the next launch.
    auto regenerateOrFail = [&](const char* reason) {
        if (!vaultIsDurable) {
            qFatal("%s and no persistent OS credential vault is available to safely generate a "
                   "replacement that would survive a restart", reason);
        }
        createCredentials();
        fromVault = true;
    };

    if (m_CachedPemCert.isEmpty() || m_CachedPrivateKey.isEmpty()) {
        qInfo() << "No existing credentials found";
        regenerateOrFail("No existing pairing identity was found");
    }
    else if (getSslCertificate().isNull()) {
        qWarning() << "Certificate is unreadable";
        regenerateOrFail("The stored pairing certificate is unreadable");
    }
    else if (getSslKey().isNull()) {
        qWarning() << "Private key is unreadable";
        regenerateOrFail("The stored pairing private key is unreadable");
    }

    // We should have valid credentials now. If not, we're screwed
    if (getSslCertificate().isNull()) {
        qFatal("Certificate is unreadable");
    }
    if (getSslKey().isNull()) {
        qFatal("Private key is unreadable");
    }

    // Commit the marker only after a durable vault holds a valid identity.
    // Then remove plaintext legacy keys. If the process crashes before the
    // marker or removal, the next launch safely retries the idempotent import.
    if (!migrationRecorded && vaultIsDurable && fromVault
            && database && database->isOpen()) {
        database->setSetting(QStringLiteral(IDENTITY_MIGRATION_MARKER), true);
        // setSetting() has no return value; read the marker back to
        // confirm it was actually persisted before treating the legacy
        // QSettings bytes below as safe to remove.
        migrationRecorded =
            database->setting(QStringLiteral(IDENTITY_MIGRATION_MARKER), false).toBool();
    }
    if (migrationRecorded && vaultIsDurable && fromVault) {
        settings.remove(QStringLiteral(SER_CERT));
        settings.remove(QStringLiteral(SER_KEY));
        settings.sync();
        if (settings.status() != QSettings::NoError) {
            qWarning() << "Could not remove legacy plaintext pairing credentials from QSettings";
        }
    }

    // Load the unique ID from settings
    m_CachedUniqueId = settings.value(SER_UNIQUEID).toString();
    if (!m_CachedUniqueId.isEmpty()) {
        qInfo() << "Loaded existing Client identity";
    }
    else {
        // Generate a new unique ID in base 16
        uint64_t uid;
        RAND_bytes(reinterpret_cast<unsigned char*>(&uid), sizeof(uid));
        m_CachedUniqueId = QString::number(uid, 16);

        qInfo() << "Generated new Client identity";

        settings.setValue(SER_UNIQUEID, m_CachedUniqueId);
    }
}

QSslCertificate
IdentityManager::getSslCertificate()
{
    if (m_CachedSslCert.isNull()) {
        m_CachedSslCert = QSslCertificate(m_CachedPemCert);
    }
    return m_CachedSslCert;
}

QSslKey
IdentityManager::getSslKey()
{
    if (m_CachedSslKey.isNull()) {
        // This seemingly useless const_cast is required for old OpenSSL headers
        // where BIO_new_mem_buf's parameter is not declared const like those on
        // the Steam Link hardware.
        BIO* bio = BIO_new_mem_buf(const_cast<char*>(m_CachedPrivateKey.constData()), -1);
        THROW_BAD_ALLOC_IF_NULL(bio);

        EVP_PKEY* pk = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
        BIO_free(bio);

        bio = BIO_new(BIO_s_mem());
        THROW_BAD_ALLOC_IF_NULL(bio);

        // We must write out our PEM in the old PKCS1 format for SecureTransport
        // on macOS/iOS to be able to read it.
#ifdef Q_OS_DARWIN
        PEM_write_bio_PrivateKey_traditional(bio, pk, nullptr, nullptr, 0, nullptr, 0);
#else
        PEM_write_bio_PrivateKey(bio, pk, nullptr, nullptr, 0, nullptr, 0);
#endif

        BUF_MEM* mem;
        BIO_get_mem_ptr(bio, &mem);
        m_CachedSslKey = QSslKey(QByteArray(mem->data, (int)mem->length), QSsl::Rsa);

        BIO_free(bio);
        EVP_PKEY_free(pk);
    }
    return m_CachedSslKey;
}

QSslConfiguration
IdentityManager::getSslConfig()
{
    QSslConfiguration sslConfig(QSslConfiguration::defaultConfiguration());
    sslConfig.setLocalCertificate(getSslCertificate());
    sslConfig.setPrivateKey(getSslKey());
    return sslConfig;
}

QString
IdentityManager::getUniqueId()
{
    return m_CachedUniqueId;
}

QByteArray
IdentityManager::getCertificate()
{
    return m_CachedPemCert;
}

QByteArray
IdentityManager::getPrivateKey()
{
    return m_CachedPrivateKey;
}
