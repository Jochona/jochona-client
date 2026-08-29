//
// SPDX-FileCopyrightText: Lunaframe Client Contributors
//
// SPDX-License-Identifier: GPL-3.0-only
//
// Jochona: per-uuid capability cache + probe orchestrator (proposal §4.4,
// §6.9, §7.4). Same QML access pattern as ThemeManager::get() (a
// context-property singleton, not a qmlRegisterSingletonType factory --
// see thememanager.h). Deliberately takes raw connection info
// (uuid/host/port/cert) rather than an NvComputer*, so it never becomes a
// friend of NvComputer: refresh() can be driven straight from QML using the
// fields ComputerModel already exposes as roles.
//
// Capabilities are cached in QSettings under the "capabilities" group,
// keyed by host uuid, so QML has a value to bind to before the first probe
// of a session completes.
//
#pragma once

#include "hostcapabilities.h"
#include "backend/nvaddress.h"

#include <QHash>
#include <QObject>
#include <QSslCertificate>
#include <QReadWriteLock>
#include <QVariantMap>

class HostAdapterManager : public QObject
{
    Q_OBJECT

public:
    Q_INVOKABLE static HostAdapterManager* get();

    // Cached capabilities for uuid, or a default-constructed value
    // (Family::Unknown, Confidence::Unknown) if it has never been probed.
    HostCapabilities capabilities(const QString& uuid) const;

    // QML-friendly projection of capabilities(uuid); see HostCapabilities::toJson
    // for the key names (family, confidence, capabilities, permissionMask,
    // lastProbed, abrVersion, abrFeatures).
    Q_INVOKABLE QVariantMap capabilitiesFor(const QString& uuid) const;

    // True once we've heard back from this host at least once (Confirmed or
    // Partial); false before the first probe or if every probe attempt
    // failed to even reach /serverinfo.
    Q_INVOKABLE bool hasProbed(const QString& uuid) const;

    // Kicks off an async probe for the given host; capabilitiesChanged(uuid)
    // fires when it completes, successfully or not. serverCertPem is the
    // host's pinned PEM certificate (NvComputer::serverCert, PEM-encoded --
    // QSslCertificate::toPem() on the caller's side).
    Q_INVOKABLE void refresh(const QString& uuid, const QString& address, quint16 httpsPort, const QByteArray& serverCertPem);

    // C++-side convenience overload for callers that already hold Qt network types.
    void refresh(const QString& uuid, const NvAddress& address, uint16_t httpsPort, const QSslCertificate& serverCert);

    // Re-probes every host we have connection info for (used after app
    // resume/network change). A host must have been refresh()ed at least
    // once in this run for its connection info to be on file.
    Q_INVOKABLE void refreshAll();

signals:
    // Fires whenever a host's cached capabilities are replaced, whether by
    // a fresh probe result or by the initial cache load.
    void capabilitiesChanged(const QString& uuid);

private slots:
    void handleProbeFinished(QString uuid, HostCapabilities capabilities);

private:
    explicit HostAdapterManager(QObject* parent = nullptr);

    Q_DISABLE_COPY(HostAdapterManager)

    void loadCache();
    void persist(const QString& uuid, const HostCapabilities& capabilities);

    static HostAdapterManager* s_Instance;

    struct ConnectionInfo
    {
        NvAddress address;
        uint16_t httpsPort = 0;
        QSslCertificate serverCert;
    };
    // Probe results arrive on the GUI thread, but Session resolves a tuple
    // from its connection worker. Protect the cache; connection details stay
    // GUI-thread-only.
    mutable QReadWriteLock m_CacheLock;
    QHash<QString, HostCapabilities> m_Cache;
    QHash<QString, ConnectionInfo> m_ConnectionInfo;
};
