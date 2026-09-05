//
// SPDX-FileCopyrightText: Jochona Client Contributors
// SPDX-License-Identifier: GPL-3.0-only
//
#include "credentialstore.h"

#include <QDebug>

#import <CoreFoundation/CoreFoundation.h>
#import <LocalAuthentication/LocalAuthentication.h>
#import <Security/Security.h>

namespace
{
CFMutableDictionaryRef createQuery(const QString& service,
                                   const QString& account)
{
    CFMutableDictionaryRef query = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);

    CFStringRef cfService = service.toCFString();
    CFDictionarySetValue(query, kSecAttrService, cfService);
    CFRelease(cfService);

    if (!account.isNull()) {
        CFStringRef cfAccount = account.toCFString();
        CFDictionarySetValue(query, kSecAttrAccount, cfAccount);
        CFRelease(cfAccount);
    }

    LAContext* context = [[LAContext alloc] init];
    context.interactionNotAllowed = YES;
    CFDictionarySetValue(query, kSecUseAuthenticationContext,
                         (__bridge CFTypeRef)context);
#if !__has_feature(objc_arc)
    [context release];
#endif
    return query;
}
}

QString CredentialStore::backendName() const
{
    return QStringLiteral("macOS Keychain");
}

bool CredentialStore::setSecret(const QString& service,
                                const QString& account,
                                const QByteArray& secret)
{
    CFMutableDictionaryRef query = createQuery(service, account);
    CFDataRef cfSecret = CFDataCreate(
        kCFAllocatorDefault,
        reinterpret_cast<const UInt8*>(secret.constData()),
        secret.size());
    CFMutableDictionaryRef values = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(values, kSecValueData, cfSecret);
    CFDictionarySetValue(values, kSecAttrAccessible,
                         kSecAttrAccessibleAfterFirstUnlock);

    OSStatus status = SecItemUpdate(query, values);
    if (status == errSecItemNotFound) {
        CFDictionaryRemoveValue(query, kSecUseAuthenticationContext);
        CFDictionarySetValue(query, kSecValueData, cfSecret);
        CFDictionarySetValue(query, kSecAttrAccessible,
                             kSecAttrAccessibleAfterFirstUnlock);
        status = SecItemAdd(query, nullptr);
    }

    CFRelease(values);
    CFRelease(cfSecret);
    CFRelease(query);

    if (status != errSecSuccess) {
        qWarning() << "CredentialStore: Keychain write failed with status"
                   << status << "for service" << service;
        return false;
    }
    return true;
}

QByteArray CredentialStore::getSecret(const QString& service,
                                      const QString& account) const
{
    CFMutableDictionaryRef query = createQuery(service, account);
    CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitOne);

    CFTypeRef result = nullptr;
    const OSStatus status = SecItemCopyMatching(query, &result);
    CFRelease(query);

    if (status != errSecSuccess && status != errSecItemNotFound) {
        qWarning() << "CredentialStore: Keychain read failed with status"
                   << status << "for service" << service;
    }
    if (status != errSecSuccess || result == nullptr) {
        return {};
    }

    CFDataRef data = static_cast<CFDataRef>(result);
    const QByteArray secret(
        reinterpret_cast<const char*>(CFDataGetBytePtr(data)),
        static_cast<int>(CFDataGetLength(data)));
    CFRelease(result);
    return secret;
}

bool CredentialStore::hasSecret(const QString& service,
                                const QString& account) const
{
    CFMutableDictionaryRef query = createQuery(service, account);
    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitOne);
    const OSStatus status = SecItemCopyMatching(query, nullptr);
    CFRelease(query);
    return status == errSecSuccess;
}

bool CredentialStore::removeSecret(const QString& service,
                                   const QString& account)
{
    CFMutableDictionaryRef query = createQuery(service, account);
    const OSStatus status = SecItemDelete(query);
    CFRelease(query);
    return status == errSecSuccess || status == errSecItemNotFound;
}

QStringList CredentialStore::listAccounts(const QString& service) const
{
    CFMutableDictionaryRef query = createQuery(service, QString());
    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitAll);
    CFDictionarySetValue(query, kSecReturnAttributes, kCFBooleanTrue);

    CFTypeRef result = nullptr;
    const OSStatus status = SecItemCopyMatching(query, &result);
    CFRelease(query);

    QStringList accounts;
    if (status == errSecSuccess && result != nullptr) {
        CFArrayRef items = static_cast<CFArrayRef>(result);
        const CFIndex count = CFArrayGetCount(items);
        for (CFIndex i = 0; i < count; i++) {
            CFDictionaryRef item = static_cast<CFDictionaryRef>(
                CFArrayGetValueAtIndex(items, i));
            CFStringRef accountRef = static_cast<CFStringRef>(
                CFDictionaryGetValue(item, kSecAttrAccount));
            if (accountRef != nullptr) {
                accounts.append(QString::fromCFString(accountRef));
            }
        }
        CFRelease(result);
    }
    return accounts;
}
