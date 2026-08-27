#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>

#if defined(Q_OS_LINUX)
#include <QHash>
#include <QVector>
#endif

// CredentialStore persists secrets (the pairing certificate/private key,
// future provider tokens) in the operating system's credential vault
// instead of plaintext QSettings or the SettingsDatabase (proposal.md 6.4,
// 7.1). Each secret is keyed by a (service, account) pair, mirroring the
// vocabulary every backend already uses natively:
//
//  - macOS: Keychain generic passwords via SecItem (kSecClassGenericPassword).
//  - Windows: Credential Manager via wincred (CRED_TYPE_GENERIC).
//  - Linux: the Secret Service via libsecret, resolved at runtime with
//    dlopen()/dlsym() against libsecret-1.so.0 -- there is no build-time or
//    link-time dependency on libsecret. When libsecret cannot be loaded
//    (missing package, no Secret Service running), CredentialStore falls
//    back to an in-memory store and logs a warning; secrets never touch
//    disk in plaintext on any platform.
class CredentialStore : public QObject
{
    Q_OBJECT

public:
    explicit CredentialStore(QObject* parent = nullptr);

    ~CredentialStore() override;

    Q_INVOKABLE bool setSecret(const QString& service, const QString& account, const QByteArray& secret);

    Q_INVOKABLE QByteArray getSecret(const QString& service, const QString& account) const;

    Q_INVOKABLE bool hasSecret(const QString& service, const QString& account) const;

    Q_INVOKABLE bool removeSecret(const QString& service, const QString& account);

    Q_INVOKABLE QStringList listAccounts(const QString& service) const;

    // Human-readable name of the active backend, for diagnostics only.
    QString backendName() const;

private:
#if defined(Q_OS_LINUX)
    // Used only when libsecret could not be loaded. Never persisted;
    // secrets are lost when the process exits.
    struct MemoryEntry
    {
        QString account;
        QByteArray secret;
    };

    bool memorySetSecret(const QString& service, const QString& account, const QByteArray& secret);
    QByteArray memoryGetSecret(const QString& service, const QString& account) const;
    bool memoryHasSecret(const QString& service, const QString& account) const;
    bool memoryRemoveSecret(const QString& service, const QString& account);
    QStringList memoryListAccounts(const QString& service) const;

    void updateLinuxIndex(const QString& service, const QString& account, bool added);
    QStringList linuxIndexAccounts(const QString& service) const;

    mutable QHash<QString, QVector<MemoryEntry>> m_MemoryStore;
#endif
};
