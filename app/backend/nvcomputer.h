#pragma once

#include "nvhttp.h"
#include "nvaddress.h"

#include <QThread>
#include <QReadWriteLock>
#include <QSettings>
#include <QRunnable>

class CopySafeReadWriteLock : public QReadWriteLock
{
public:
    CopySafeReadWriteLock() = default;

    // Don't actually copy the QReadWriteLock
    CopySafeReadWriteLock(const CopySafeReadWriteLock&) : QReadWriteLock() {}
    CopySafeReadWriteLock& operator=(const CopySafeReadWriteLock &) { return *this; }
};

class NvComputer
{
    friend class PcMonitorThread;
    friend class ComputerManager;
    friend class PendingQuitTask;

private:
    void sortAppList();

    bool updateAppList(QVector<NvApp> newAppList);

    bool pendingQuit;

public:
    NvComputer() = default;

    // Caller is responsible for synchronizing read access to the other host
    NvComputer(const NvComputer&) = default;

    // Caller is responsible for synchronizing read access to the other host
    NvComputer& operator=(const NvComputer &) = default;

    explicit NvComputer(NvHTTP& http, QString serverInfo);

    explicit NvComputer(QSettings& settings);

    void
    setRemoteAddress(QHostAddress);

    bool
    update(const NvComputer& that);

    bool
    wake() const;

    enum ReachabilityType
    {
        RI_UNKNOWN,
        RI_LAN,
        RI_VPN,
        // Active path is a Tailscale/tailnet address (CGNAT 100.64/10 or a
        // detected ts* interface). Reported separately so the UI can label
        // "Tailnet" instead of a generic VPN badge.
        RI_TAILNET
    };

    ReachabilityType
    getActiveAddressReachability() const;

    QVector<NvAddress>
    uniqueAddresses() const;

    void
    serialize(QSettings& settings, bool serializeApps) const;

    // Caller is responsible for synchronizing read access to both hosts
    bool
    isEqualSerialized(const NvComputer& that) const;

    enum PairState
    {
        PS_UNKNOWN,
        PS_PAIRED,
        PS_NOT_PAIRED
    };

    enum ComputerState
    {
        CS_UNKNOWN,
        CS_ONLINE,
        CS_OFFLINE
    };

    // Ephemeral traits
    ComputerState state;
    // Cached path classification kept fresh by the polling thread; safe and
    // cheap for the UI to display (the real probe does network I/O).
    ReachabilityType activeReachability;
    PairState pairState;
    NvAddress activeAddress;
    uint16_t activeHttpsPort;
    int currentGameId;
    QString gfeVersion;
    QString appVersion;
    QVector<NvDisplayMode> displayModes;
    int maxLumaPixelsHEVC;
    int serverCodecModeSupport;
    QString gpuModel;
    bool isSupportedServerVersion;

    // Persisted traits
    NvAddress localAddress;
    NvAddress remoteAddress;
    NvAddress ipv6Address;
    NvAddress manualAddress;
    QByteArray macAddress;
    // User-configured Wake-on-LAN overrides (proposal §6.5). All optional;
    // empty/zero means "use learned behavior". manualMacAddress wins over
    // the learned macAddress when set (NIC MAC rotated or was never sent).
    QByteArray manualMacAddress;
    quint16 wakePort;             // 0 = standard+dynamic port sweep
    QString wakeBroadcastAddress; // empty = sweep all NIC broadcasts/multicasts
    QString name;
    bool hasCustomName;
    QString uuid;
    QSslCertificate serverCert;
    // ADR-0007: an unexpected change to the pinned certificate enters Trust
    // state Identity Changed and hard-blocks Sessions until the user
    // deliberately re-pairs (NvPairingManager::pair() writes serverCert
    // directly and is the only path that may clear this). update() sets
    // this instead of silently overwriting serverCert when a live probe
    // observes a different certificate for an already-established host;
    // pendingServerCert holds that newly observed certificate so the UI can
    // show the old/new identity facts before an explicit re-pair.
    bool identityChanged = false;
    QSslCertificate pendingServerCert;
    QVector<NvApp> appList;
    bool isNvidiaServerSoftware;
    // Remember to update isEqualSerialized() when adding fields here!

    // Synchronization
    mutable CopySafeReadWriteLock lock;

private:
    uint16_t externalPort;
};
