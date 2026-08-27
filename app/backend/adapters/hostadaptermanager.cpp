//
// SPDX-FileCopyrightText: Lunaframe Client Contributors
//
// SPDX-License-Identifier: GPL-3.0-only
//
#include "hostadaptermanager.h"
#include "hostprober.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QThreadPool>

#define SER_GROUP "capabilities"

HostAdapterManager* HostAdapterManager::s_Instance = nullptr;

HostAdapterManager::HostAdapterManager(QObject* parent)
    : QObject(parent)
{
    // Required once before the first cross-thread capabilitiesReady queued
    // connection below can carry a HostCapabilities argument.
    qRegisterMetaType<HostCapabilities>("HostCapabilities");

    loadCache();
}

HostAdapterManager*
HostAdapterManager::get()
{
    if (s_Instance == nullptr) {
        s_Instance = new HostAdapterManager();
    }

    return s_Instance;
}

void
HostAdapterManager::loadCache()
{
    QSettings settings;

    settings.beginGroup(SER_GROUP);
    const QStringList uuids = settings.childKeys();
    for (const QString& uuid : uuids) {
        const QByteArray json = settings.value(uuid).toByteArray();

        QJsonParseError parseError {};
        const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            continue;
        }

        m_Cache.insert(uuid, HostCapabilities::fromJson(doc.object()));
    }
    settings.endGroup();
}

void
HostAdapterManager::persist(const QString& uuid, const HostCapabilities& capabilities)
{
    QSettings settings;

    settings.beginGroup(SER_GROUP);
    settings.setValue(uuid, QJsonDocument(capabilities.toJson()).toJson(QJsonDocument::Compact));
    settings.endGroup();
}

HostCapabilities
HostAdapterManager::capabilities(const QString& uuid) const
{
    return m_Cache.value(uuid);
}

QVariantMap
HostAdapterManager::capabilitiesFor(const QString& uuid) const
{
    return m_Cache.value(uuid).toJson().toVariantMap();
}

bool
HostAdapterManager::hasProbed(const QString& uuid) const
{
    const auto it = m_Cache.constFind(uuid);
    return it != m_Cache.constEnd() && it->confidence != HostCapabilities::Confidence::Unknown;
}

void
HostAdapterManager::refresh(const QString& uuid, const QString& address, quint16 httpsPort, const QByteArray& serverCertPem)
{
    refresh(uuid, NvAddress(address, httpsPort), httpsPort, QSslCertificate(serverCertPem));
}

void
HostAdapterManager::refresh(const QString& uuid, const NvAddress& address, uint16_t httpsPort, const QSslCertificate& serverCert)
{
    ConnectionInfo info;
    info.address = address;
    info.httpsPort = httpsPort;
    info.serverCert = serverCert;
    m_ConnectionInfo.insert(uuid, info);

    // HostProber is created here on the GUI thread; QThreadPool runs it on
    // a worker thread, and its capabilitiesReady signal is delivered back
    // to us via a queued connection (see hostprober.h).
    HostProber* prober = new HostProber(uuid, address, httpsPort, serverCert);
    connect(prober, &HostProber::capabilitiesReady, this, &HostAdapterManager::handleProbeFinished);
    QThreadPool::globalInstance()->start(prober);
}

void
HostAdapterManager::refreshAll()
{
    const QStringList uuids = m_ConnectionInfo.keys();
    for (const QString& uuid : uuids) {
        const ConnectionInfo& info = m_ConnectionInfo.value(uuid);
        refresh(uuid, info.address, info.httpsPort, info.serverCert);
    }
}

void
HostAdapterManager::handleProbeFinished(QString uuid, HostCapabilities capabilities)
{
    m_Cache.insert(uuid, capabilities);
    persist(uuid, capabilities);

    emit capabilitiesChanged(uuid);
}
