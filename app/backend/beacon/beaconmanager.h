//
// SPDX-FileCopyrightText: Jochona Client Contributors
// SPDX-License-Identifier: GPL-3.0-only
//
#pragma once

#include <QHash>
#include <QObject>
#include <QReadWriteLock>
#include <QSharedPointer>
#include <QVariantList>
#include <QVariantMap>

#include <qmdnsengine/browser.h>
#include <qmdnsengine/server.h>
#include <qmdnsengine/service.h>

class BeaconManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList pairedBeacons READ pairedBeacons
               NOTIFY beaconsChanged)
    Q_PROPERTY(QVariantList discoveredBeacons READ discoveredBeacons
               NOTIFY discoveredBeaconsChanged)
    Q_PROPERTY(bool pairing READ pairing NOTIFY pairingChanged)

public:
    static BeaconManager* get();

    QVariantList pairedBeacons() const;
    QVariantList discoveredBeacons() const;
    bool pairing() const;

    Q_INVOKABLE void pairBeacon(const QString& url,
                                const QString& shortCode,
                                const QString& displayName = QString());
    Q_INVOKABLE bool removeBeacon(const QString& beaconId);
    Q_INVOKABLE void refreshHosts(const QString& beaconId);
    Q_INVOKABLE QVariantList hostsForBeacon(const QString& beaconId) const;
    Q_INVOKABLE QVariantMap wakeRouteForHost(const QString& hostId) const;
    Q_INVOKABLE bool setDirectWake(const QString& hostId);
    Q_INVOKABLE bool setBeaconWake(const QString& hostId,
                                   const QString& beaconId,
                                   const QString& beaconHostId);

    bool hasBeaconRoute(const QString& hostId) const;
    bool dispatchWake(const QString& hostId,
                      QString* wakeId = nullptr,
                      QString* error = nullptr);

signals:
    void beaconsChanged();
    void discoveredBeaconsChanged();
    void pairingChanged();
    void pairingFinished(bool success, QString error, QString beaconId);
    void hostsChanged(QString beaconId);
    void hostRefreshFailed(QString beaconId, QString error);
    void beaconIdentityChanged(QString beaconId);

private slots:
    void handlePairingFinished(bool success,
                               QString error,
                               QVariantMap beacon);
    void handleHostsFinished(QString beaconId,
                             QVariantList hosts,
                             QString error,
                             bool identityChanged);
    void handleIdentityChanged(const QString& beaconId);

private:
    explicit BeaconManager(QObject* parent = nullptr);
    Q_DISABLE_COPY(BeaconManager)

    struct BeaconRecord
    {
        QString id;
        QString name;
        QString url;
        QByteArray spkiFingerprint;
        QString identityState;
    };

    struct WakeRoute
    {
        QString provider = QStringLiteral("direct");
        QString beaconId;
        QString beaconHostId;
    };

    void loadCache();
    void startDiscovery();
    static QVariantMap recordToVariant(const BeaconRecord& record);

    static BeaconManager* s_Instance;

    mutable QReadWriteLock m_Lock;
    QHash<QString, BeaconRecord> m_Beacons;
    QHash<QString, WakeRoute> m_Routes;
    QHash<QString, QVariantList> m_Hosts;
    QHash<QString, QVariantMap> m_Discovered;
    bool m_Pairing = false;

    QSharedPointer<QMdnsEngine::Server> m_MdnsServer;
    QMdnsEngine::Browser* m_MdnsBrowser = nullptr;
};
