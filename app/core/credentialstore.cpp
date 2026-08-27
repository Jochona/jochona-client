#include "credentialstore.h"

#include <QtDebug>

CredentialStore::CredentialStore(QObject* parent)
    : QObject(parent)
{
}

CredentialStore::~CredentialStore()
{
}

#if defined(Q_OS_DARWIN)

// ---------------------------------------------------------------------
// macOS backend: Keychain generic passwords via SecItem.
// ---------------------------------------------------------------------

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

namespace {
    CFMutableDictionaryRef createQuery(const QString& service, const QString& account)
    {
        CFMutableDictionaryRef query = CFDictionaryCreateMutable(
                kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);

        CFStringRef cfService = service.toCFString();
        CFDictionarySetValue(query, kSecAttrService, cfService);
        CFRelease(cfService);

        if (!account.isNull()) {
            CFStringRef cfAccount = account.toCFString();
            CFDictionarySetValue(query, kSecAttrAccount, cfAccount);
            CFRelease(cfAccount);
        }

        return query;
    }
}

QString CredentialStore::backendName() const
{
    return QStringLiteral("macOS Keychain");
}

bool CredentialStore::setSecret(const QString& service, const QString& account, const QByteArray& secret)
{
    CFMutableDictionaryRef query = createQuery(service, account);

    // SecItemAdd fails if a matching item already exists, so make this an
    // upsert by deleting any prior value first.
    SecItemDelete(query);

    CFDataRef cfSecret = CFDataCreate(kCFAllocatorDefault, reinterpret_cast<const UInt8*>(secret.constData()), secret.size());
    CFDictionarySetValue(query, kSecValueData, cfSecret);
    CFDictionarySetValue(query, kSecAttrAccessible, kSecAttrAccessibleAfterFirstUnlock);

    OSStatus status = SecItemAdd(query, nullptr);

    CFRelease(cfSecret);
    CFRelease(query);

    if (status != errSecSuccess) {
        qWarning() << "CredentialStore: SecItemAdd failed with status" << status << "for service" << service;
        return false;
    }
    return true;
}

QByteArray CredentialStore::getSecret(const QString& service, const QString& account) const
{
    CFMutableDictionaryRef query = createQuery(service, account);
    CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitOne);

    CFTypeRef result = nullptr;
    OSStatus status = SecItemCopyMatching(query, &result);
    CFRelease(query);

    QByteArray secret;
    if (status == errSecSuccess && result) {
        CFDataRef data = static_cast<CFDataRef>(result);
        secret = QByteArray(reinterpret_cast<const char*>(CFDataGetBytePtr(data)), static_cast<int>(CFDataGetLength(data)));
        CFRelease(result);
    }
    return secret;
}

bool CredentialStore::hasSecret(const QString& service, const QString& account) const
{
    CFMutableDictionaryRef query = createQuery(service, account);
    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitOne);

    OSStatus status = SecItemCopyMatching(query, nullptr);
    CFRelease(query);

    return status == errSecSuccess;
}

bool CredentialStore::removeSecret(const QString& service, const QString& account)
{
    CFMutableDictionaryRef query = createQuery(service, account);
    OSStatus status = SecItemDelete(query);
    CFRelease(query);

    return status == errSecSuccess || status == errSecItemNotFound;
}

QStringList CredentialStore::listAccounts(const QString& service) const
{
    CFMutableDictionaryRef query = createQuery(service, QString());
    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitAll);
    CFDictionarySetValue(query, kSecReturnAttributes, kCFBooleanTrue);

    CFTypeRef result = nullptr;
    OSStatus status = SecItemCopyMatching(query, &result);
    CFRelease(query);

    QStringList accounts;
    if (status == errSecSuccess && result) {
        CFArrayRef items = static_cast<CFArrayRef>(result);
        const CFIndex count = CFArrayGetCount(items);
        for (CFIndex i = 0; i < count; i++) {
            CFDictionaryRef item = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(items, i));
            CFStringRef accountRef = static_cast<CFStringRef>(CFDictionaryGetValue(item, kSecAttrAccount));
            if (accountRef) {
                accounts.append(QString::fromCFString(accountRef));
            }
        }
        CFRelease(result);
    }
    return accounts;
}

#elif defined(Q_OS_WIN32)

// ---------------------------------------------------------------------
// Windows backend: Credential Manager via wincred.
// ---------------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <wincred.h>

namespace {
    // Credential Manager has a single flat namespace, so accounts are
    // encoded into TargetName under a Jochona-owned prefix to avoid
    // colliding with credentials other applications (or the user) create.
    QString composeTargetName(const QString& service, const QString& account)
    {
        return QStringLiteral("Jochona:%1:%2").arg(service, account);
    }

    QString targetPrefix(const QString& service)
    {
        return QStringLiteral("Jochona:%1:").arg(service);
    }
}

QString CredentialStore::backendName() const
{
    return QStringLiteral("Windows Credential Manager");
}

bool CredentialStore::setSecret(const QString& service, const QString& account, const QByteArray& secret)
{
    const QString target = composeTargetName(service, account);
    const std::wstring targetName = target.toStdWString();
    const std::wstring userName = account.toStdWString();

    CREDENTIALW cred = {};
    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = const_cast<LPWSTR>(targetName.c_str());
    cred.CredentialBlobSize = static_cast<DWORD>(secret.size());
    cred.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char*>(secret.constData()));
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
    cred.UserName = const_cast<LPWSTR>(userName.c_str());

    if (!CredWriteW(&cred, 0)) {
        qWarning() << "CredentialStore: CredWriteW failed with error" << GetLastError() << "for service" << service;
        return false;
    }
    return true;
}

QByteArray CredentialStore::getSecret(const QString& service, const QString& account) const
{
    const std::wstring targetName = composeTargetName(service, account).toStdWString();

    PCREDENTIALW cred = nullptr;
    QByteArray secret;
    if (CredReadW(targetName.c_str(), CRED_TYPE_GENERIC, 0, &cred)) {
        secret = QByteArray(reinterpret_cast<const char*>(cred->CredentialBlob), static_cast<int>(cred->CredentialBlobSize));
        CredFree(cred);
    }
    return secret;
}

bool CredentialStore::hasSecret(const QString& service, const QString& account) const
{
    const std::wstring targetName = composeTargetName(service, account).toStdWString();

    PCREDENTIALW cred = nullptr;
    const bool found = CredReadW(targetName.c_str(), CRED_TYPE_GENERIC, 0, &cred);
    if (found) {
        CredFree(cred);
    }
    return found;
}

bool CredentialStore::removeSecret(const QString& service, const QString& account)
{
    const std::wstring targetName = composeTargetName(service, account).toStdWString();

    if (CredDeleteW(targetName.c_str(), CRED_TYPE_GENERIC, 0)) {
        return true;
    }
    return GetLastError() == ERROR_NOT_FOUND;
}

QStringList CredentialStore::listAccounts(const QString& service) const
{
    const QString prefix = targetPrefix(service);
    const std::wstring filter = (prefix + QStringLiteral("*")).toStdWString();

    DWORD count = 0;
    PCREDENTIALW* credentials = nullptr;
    QStringList accounts;
    if (CredEnumerateW(filter.c_str(), 0, &count, &credentials)) {
        for (DWORD i = 0; i < count; i++) {
            const QString target = QString::fromWCharArray(credentials[i]->TargetName);
            if (target.startsWith(prefix)) {
                accounts.append(target.mid(prefix.length()));
            }
        }
        CredFree(credentials);
    }
    return accounts;
}

#elif defined(Q_OS_LINUX)

// ---------------------------------------------------------------------
// Linux backend: the Secret Service via libsecret, loaded at runtime.
//
// We deliberately avoid linking against libsecret (and therefore GLib) at
// build time: the shared object is resolved with dlopen() and its "simple
// password API" functions with dlsym(), so the binary loads and degrades
// gracefully (falling back to an in-memory store) on systems without
// libsecret or a running Secret Service, and building this file never
// requires libsecret-dev to be installed.
//
// Because the simple API has no bulk enumeration entry point, each service
// also gets a reserved "index" secret (kIndexAccountName) holding a JSON
// array of account names, which is what listAccounts() reads.
// ---------------------------------------------------------------------

#include <dlfcn.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>

namespace {
    constexpr const char* kIndexAccountName = "__jochona_account_index__";

    // Mirrors libsecret's public, ABI-stable SecretSchema (libsecret/secret-schema.h)
    // so we can call into the library without its headers or GLib's.
    struct LibSecretSchemaAttribute
    {
        const char* name;
        int type; // SECRET_SCHEMA_ATTRIBUTE_STRING == 0
    };

    struct LibSecretSchema
    {
        const char* name;
        int flags; // SECRET_SCHEMA_NONE == 0
        LibSecretSchemaAttribute attributes[32];
        int reserved;
        void* reserved1;
        void* reserved2;
        void* reserved3;
        void* reserved4;
        void* reserved5;
        void* reserved6;
        void* reserved7;
    };

    using SecretPasswordStoreSyncFn = int (*)(const LibSecretSchema*, const char*, const char*, const char*, void*, void*, ...);
    using SecretPasswordLookupSyncFn = char* (*)(const LibSecretSchema*, void*, void*, ...);
    using SecretPasswordClearSyncFn = int (*)(const LibSecretSchema*, void*, void*, ...);
    using SecretPasswordFreeFn = void (*)(char*);

    class LibSecretBinding
    {
    public:
        static LibSecretBinding& instance()
        {
            static LibSecretBinding binding;
            return binding;
        }

        bool isAvailable() const { return m_StoreSync && m_LookupSync && m_ClearSync && m_Free; }

        SecretPasswordStoreSyncFn storeSync() const { return m_StoreSync; }
        SecretPasswordLookupSyncFn lookupSync() const { return m_LookupSync; }
        SecretPasswordClearSyncFn clearSync() const { return m_ClearSync; }
        SecretPasswordFreeFn freeString() const { return m_Free; }

    private:
        LibSecretBinding()
        {
            static const char* kLibraryNames[] = { "libsecret-1.so.0", "libsecret-1.so" };
            for (const char* name : kLibraryNames) {
                m_Handle = dlopen(name, RTLD_NOW | RTLD_LOCAL);
                if (m_Handle) {
                    break;
                }
            }

            if (!m_Handle) {
                qWarning() << "CredentialStore: libsecret is not available; falling back to an in-memory "
                              "secret store. Secrets will not survive a process restart.";
                return;
            }

            m_StoreSync = reinterpret_cast<SecretPasswordStoreSyncFn>(dlsym(m_Handle, "secret_password_store_sync"));
            m_LookupSync = reinterpret_cast<SecretPasswordLookupSyncFn>(dlsym(m_Handle, "secret_password_lookup_sync"));
            m_ClearSync = reinterpret_cast<SecretPasswordClearSyncFn>(dlsym(m_Handle, "secret_password_clear_sync"));
            m_Free = reinterpret_cast<SecretPasswordFreeFn>(dlsym(m_Handle, "secret_password_free"));

            if (!isAvailable()) {
                qWarning() << "CredentialStore: libsecret is missing expected symbols; falling back to an "
                              "in-memory secret store.";
                dlclose(m_Handle);
                m_Handle = nullptr;
                m_StoreSync = nullptr;
                m_LookupSync = nullptr;
                m_ClearSync = nullptr;
                m_Free = nullptr;
            }
        }

        ~LibSecretBinding()
        {
            if (m_Handle) {
                dlclose(m_Handle);
            }
        }

        Q_DISABLE_COPY(LibSecretBinding)

        void* m_Handle = nullptr;
        SecretPasswordStoreSyncFn m_StoreSync = nullptr;
        SecretPasswordLookupSyncFn m_LookupSync = nullptr;
        SecretPasswordClearSyncFn m_ClearSync = nullptr;
        SecretPasswordFreeFn m_Free = nullptr;
    };

    const LibSecretSchema& jochonaSchema()
    {
        static const LibSecretSchema schema = [] {
            LibSecretSchema s = {};
            s.name = "com.jochona.client.Credential";
            s.flags = 0; // SECRET_SCHEMA_NONE
            s.attributes[0] = { "service", 0 }; // SECRET_SCHEMA_ATTRIBUTE_STRING
            s.attributes[1] = { "account", 0 };
            s.attributes[2] = { nullptr, 0 };
            return s;
        }();
        return schema;
    }
}

QString CredentialStore::backendName() const
{
    return LibSecretBinding::instance().isAvailable() ? QStringLiteral("libsecret") : QStringLiteral("in-memory fallback");
}

bool CredentialStore::setSecret(const QString& service, const QString& account, const QByteArray& secret)
{
    if (account == QLatin1String(kIndexAccountName)) {
        return false;
    }

    auto& lib = LibSecretBinding::instance();
    if (!lib.isAvailable()) {
        return memorySetSecret(service, account, secret);
    }

    const QByteArray serviceUtf8 = service.toUtf8();
    const QByteArray accountUtf8 = account.toUtf8();
    const QByteArray encoded = secret.toBase64();
    const QByteArray label = (service + QStringLiteral(" - ") + account).toUtf8();

    const int ok = lib.storeSync()(&jochonaSchema(), "default", label.constData(), encoded.constData(),
                                    nullptr, nullptr,
                                    "service", serviceUtf8.constData(),
                                    "account", accountUtf8.constData(),
                                    static_cast<const char*>(nullptr));
    if (!ok) {
        qWarning() << "CredentialStore: failed to store secret via libsecret for service" << service;
        return false;
    }

    updateLinuxIndex(service, account, true);
    return true;
}

QByteArray CredentialStore::getSecret(const QString& service, const QString& account) const
{
    if (account == QLatin1String(kIndexAccountName)) {
        return QByteArray();
    }

    auto& lib = LibSecretBinding::instance();
    if (!lib.isAvailable()) {
        return memoryGetSecret(service, account);
    }

    const QByteArray serviceUtf8 = service.toUtf8();
    const QByteArray accountUtf8 = account.toUtf8();

    char* raw = lib.lookupSync()(&jochonaSchema(), nullptr, nullptr,
                                  "service", serviceUtf8.constData(),
                                  "account", accountUtf8.constData(),
                                  static_cast<const char*>(nullptr));
    if (!raw) {
        return QByteArray();
    }

    const QByteArray encoded(raw);
    lib.freeString()(raw);
    return QByteArray::fromBase64(encoded);
}

bool CredentialStore::hasSecret(const QString& service, const QString& account) const
{
    if (account == QLatin1String(kIndexAccountName)) {
        return false;
    }

    auto& lib = LibSecretBinding::instance();
    if (!lib.isAvailable()) {
        return memoryHasSecret(service, account);
    }

    const QByteArray serviceUtf8 = service.toUtf8();
    const QByteArray accountUtf8 = account.toUtf8();

    char* raw = lib.lookupSync()(&jochonaSchema(), nullptr, nullptr,
                                  "service", serviceUtf8.constData(),
                                  "account", accountUtf8.constData(),
                                  static_cast<const char*>(nullptr));
    if (!raw) {
        return false;
    }
    lib.freeString()(raw);
    return true;
}

bool CredentialStore::removeSecret(const QString& service, const QString& account)
{
    if (account == QLatin1String(kIndexAccountName)) {
        return false;
    }

    auto& lib = LibSecretBinding::instance();
    if (!lib.isAvailable()) {
        return memoryRemoveSecret(service, account);
    }

    const QByteArray serviceUtf8 = service.toUtf8();
    const QByteArray accountUtf8 = account.toUtf8();

    lib.clearSync()(&jochonaSchema(), nullptr, nullptr,
                     "service", serviceUtf8.constData(),
                     "account", accountUtf8.constData(),
                     static_cast<const char*>(nullptr));

    updateLinuxIndex(service, account, false);
    return true;
}

QStringList CredentialStore::listAccounts(const QString& service) const
{
    auto& lib = LibSecretBinding::instance();
    if (!lib.isAvailable()) {
        return memoryListAccounts(service);
    }
    return linuxIndexAccounts(service);
}

void CredentialStore::updateLinuxIndex(const QString& service, const QString& account, bool added)
{
    auto& lib = LibSecretBinding::instance();
    if (!lib.isAvailable()) {
        return;
    }

    QStringList accounts = linuxIndexAccounts(service);
    if (added) {
        if (!accounts.contains(account)) {
            accounts.append(account);
        }
    } else {
        accounts.removeAll(account);
    }

    QJsonArray array;
    for (const QString& a : accounts) {
        array.append(a);
    }
    const QByteArray json = QJsonDocument(array).toJson(QJsonDocument::Compact);

    const QByteArray serviceUtf8 = service.toUtf8();
    const QByteArray indexAccount(kIndexAccountName);
    const QByteArray encoded = json.toBase64();
    const QByteArray label = (service + QStringLiteral(" - account index")).toUtf8();

    lib.storeSync()(&jochonaSchema(), "default", label.constData(), encoded.constData(),
                     nullptr, nullptr,
                     "service", serviceUtf8.constData(),
                     "account", indexAccount.constData(),
                     static_cast<const char*>(nullptr));
}

QStringList CredentialStore::linuxIndexAccounts(const QString& service) const
{
    auto& lib = LibSecretBinding::instance();
    const QByteArray serviceUtf8 = service.toUtf8();
    const QByteArray indexAccount(kIndexAccountName);

    char* raw = lib.lookupSync()(&jochonaSchema(), nullptr, nullptr,
                                  "service", serviceUtf8.constData(),
                                  "account", indexAccount.constData(),
                                  static_cast<const char*>(nullptr));
    if (!raw) {
        return QStringList();
    }

    const QByteArray encoded(raw);
    lib.freeString()(raw);

    const QByteArray json = QByteArray::fromBase64(encoded);
    const QJsonDocument doc = QJsonDocument::fromJson(json);

    QStringList result;
    const QJsonArray array = doc.array();
    for (const QJsonValue& value : array) {
        result.append(value.toString());
    }
    return result;
}

bool CredentialStore::memorySetSecret(const QString& service, const QString& account, const QByteArray& secret)
{
    QVector<MemoryEntry>& entries = m_MemoryStore[service];
    for (MemoryEntry& entry : entries) {
        if (entry.account == account) {
            entry.secret = secret;
            return true;
        }
    }
    entries.append({ account, secret });
    return true;
}

QByteArray CredentialStore::memoryGetSecret(const QString& service, const QString& account) const
{
    const auto it = m_MemoryStore.constFind(service);
    if (it == m_MemoryStore.constEnd()) {
        return QByteArray();
    }
    for (const MemoryEntry& entry : it.value()) {
        if (entry.account == account) {
            return entry.secret;
        }
    }
    return QByteArray();
}

bool CredentialStore::memoryHasSecret(const QString& service, const QString& account) const
{
    const auto it = m_MemoryStore.constFind(service);
    if (it == m_MemoryStore.constEnd()) {
        return false;
    }
    for (const MemoryEntry& entry : it.value()) {
        if (entry.account == account) {
            return true;
        }
    }
    return false;
}

bool CredentialStore::memoryRemoveSecret(const QString& service, const QString& account)
{
    const auto it = m_MemoryStore.find(service);
    if (it == m_MemoryStore.end()) {
        return true;
    }
    for (int i = 0; i < it.value().size(); i++) {
        if (it.value().at(i).account == account) {
            it.value().remove(i);
            return true;
        }
    }
    return true;
}

QStringList CredentialStore::memoryListAccounts(const QString& service) const
{
    QStringList accounts;
    const auto it = m_MemoryStore.constFind(service);
    if (it != m_MemoryStore.constEnd()) {
        for (const MemoryEntry& entry : it.value()) {
            accounts.append(entry.account);
        }
    }
    return accounts;
}

#else
#error "CredentialStore has no backend for this platform"
#endif
