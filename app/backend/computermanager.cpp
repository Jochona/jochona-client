#include "computermanager.h"
#include "boxartmanager.h"
#include "core/settingsdatabase.h"
#include "nvhttp.h"
#include "nvpairingmanager.h"

#include <Limelight.h>
#include <QtEndian>

#include <QThread>
#include <QThreadPool>
#include <QCoreApplication>
#include <QRandomGenerator>
#include <atomic>

// Read only during the one-time legacy import below; paired Hosts are never
// written here again once SQLite (host_records) durably holds them.
#define SER_HOSTS "hosts"
#define SER_HOSTS_BACKUP "hostsbackup"

namespace {
    // The exact, lossless serialization of a Host's persisted fields,
    // stored as host_records.record_json (SettingsDatabase) rather than
    // QSettings. Field names mirror NvComputer::serialize()'s long-standing
    // QSettings keys for continuity; only "Persisted traits" are included,
    // matching that method -- ephemeral/live-probe state (online status,
    // GFE version, display modes, ...) is never persisted here either.
    QVariantMap hostRecordFromComputer(const NvComputer* computer)
    {
        QReadLocker lock(&computer->lock);

        QVariantMap record;
        record.insert(QStringLiteral("name"), computer->name);
        record.insert(QStringLiteral("hasCustomName"), computer->hasCustomName);
        record.insert(QStringLiteral("uuid"), computer->uuid);
        record.insert(QStringLiteral("mac"), QString::fromLatin1(computer->macAddress.toBase64()));
        record.insert(QStringLiteral("manualMac"), QString::fromLatin1(computer->manualMacAddress.toBase64()));
        record.insert(QStringLiteral("localAddress"), computer->localAddress.address());
        record.insert(QStringLiteral("localPort"), computer->localAddress.port());
        record.insert(QStringLiteral("remoteAddress"), computer->remoteAddress.address());
        record.insert(QStringLiteral("remotePort"), computer->remoteAddress.port());
        record.insert(QStringLiteral("ipv6Address"), computer->ipv6Address.address());
        record.insert(QStringLiteral("ipv6Port"), computer->ipv6Address.port());
        record.insert(QStringLiteral("manualAddress"), computer->manualAddress.address());
        record.insert(QStringLiteral("manualPort"), computer->manualAddress.port());
        record.insert(QStringLiteral("serverCert"), QString::fromLatin1(computer->serverCert.toPem()));
        // ADR-0007: Identity Changed must round-trip exactly -- silently
        // dropping or adopting pendingServerCert here would either forget
        // an unresolved identity change across a restart or (worse) make
        // an unverified certificate look trusted again.
        record.insert(QStringLiteral("identityChanged"), computer->identityChanged);
        record.insert(QStringLiteral("pendingServerCert"), QString::fromLatin1(computer->pendingServerCert.toPem()));
        record.insert(QStringLiteral("isNvidiaServerSoftware"), computer->isNvidiaServerSoftware);
        record.insert(QStringLiteral("wakePort"), computer->wakePort);
        record.insert(QStringLiteral("wakeBroadcastAddress"), computer->wakeBroadcastAddress);

        QVariantList apps;
        for (const NvApp& app : computer->appList) {
            QVariantMap appRecord;
            appRecord.insert(QStringLiteral("id"), app.id);
            appRecord.insert(QStringLiteral("name"), app.name);
            appRecord.insert(QStringLiteral("hdrSupported"), app.hdrSupported);
            appRecord.insert(QStringLiteral("isAppCollectorGame"), app.isAppCollectorGame);
            appRecord.insert(QStringLiteral("hidden"), app.hidden);
            appRecord.insert(QStringLiteral("directLaunch"), app.directLaunch);
            apps.append(appRecord);
        }
        record.insert(QStringLiteral("apps"), apps);

        return record;
    }

    NvComputer* computerFromHostRecord(const QVariantMap& record)
    {
        NvComputer* computer = new NvComputer();
        computer->name = record.value(QStringLiteral("name")).toString();
        computer->hasCustomName = record.value(QStringLiteral("hasCustomName")).toBool();
        computer->uuid = record.value(QStringLiteral("uuid")).toString();
        computer->macAddress = QByteArray::fromBase64(record.value(QStringLiteral("mac")).toString().toLatin1());
        computer->manualMacAddress = QByteArray::fromBase64(record.value(QStringLiteral("manualMac")).toString().toLatin1());
        computer->localAddress = NvAddress(record.value(QStringLiteral("localAddress")).toString(),
                                           static_cast<uint16_t>(record.value(QStringLiteral("localPort"), DEFAULT_HTTP_PORT).toUInt()));
        computer->remoteAddress = NvAddress(record.value(QStringLiteral("remoteAddress")).toString(),
                                            static_cast<uint16_t>(record.value(QStringLiteral("remotePort"), DEFAULT_HTTP_PORT).toUInt()));
        computer->ipv6Address = NvAddress(record.value(QStringLiteral("ipv6Address")).toString(),
                                          static_cast<uint16_t>(record.value(QStringLiteral("ipv6Port"), DEFAULT_HTTP_PORT).toUInt()));
        computer->manualAddress = NvAddress(record.value(QStringLiteral("manualAddress")).toString(),
                                            static_cast<uint16_t>(record.value(QStringLiteral("manualPort"), DEFAULT_HTTP_PORT).toUInt()));
        computer->serverCert = QSslCertificate(record.value(QStringLiteral("serverCert")).toString().toLatin1());
        computer->identityChanged = record.value(QStringLiteral("identityChanged")).toBool();
        computer->pendingServerCert = QSslCertificate(record.value(QStringLiteral("pendingServerCert")).toString().toLatin1());
        computer->isNvidiaServerSoftware = record.value(QStringLiteral("isNvidiaServerSoftware")).toBool();
        computer->wakePort = static_cast<quint16>(record.value(QStringLiteral("wakePort"), 0).toUInt());
        computer->wakeBroadcastAddress = record.value(QStringLiteral("wakeBroadcastAddress")).toString();

        const QVariantList apps = record.value(QStringLiteral("apps")).toList();
        computer->appList.reserve(apps.size());
        for (const QVariant& appVariant : apps) {
            const QVariantMap appRecord = appVariant.toMap();
            NvApp app;
            app.id = appRecord.value(QStringLiteral("id")).toInt();
            app.name = appRecord.value(QStringLiteral("name")).toString();
            app.hdrSupported = appRecord.value(QStringLiteral("hdrSupported")).toBool();
            app.isAppCollectorGame = appRecord.value(QStringLiteral("isAppCollectorGame")).toBool();
            app.hidden = appRecord.value(QStringLiteral("hidden")).toBool();
            app.directLaunch = appRecord.value(QStringLiteral("directLaunch")).toBool();
            computer->appList.append(app);
        }

        return computer;
    }
}

class PcMonitorThread : public QThread, public IImmediateProber
{
    Q_OBJECT

#define TRIES_BEFORE_OFFLINING 2
#define POLLS_PER_APPLIST_FETCH 10

public:
    PcMonitorThread(NvComputer* computer)
        : m_ProbeNow(false), m_Computer(computer)
    {
        setObjectName("Polling thread for " + computer->name);
    }

private:
    bool tryPollComputer(QNetworkAccessManager* nam, NvAddress address, bool& changed)
    {
        NvHTTP http(address, 0, m_Computer->serverCert, !m_Computer->isNvidiaServerSoftware, nam);

        QString serverInfo;
        try {
            serverInfo = http.getServerInfo(NvHTTP::NvLogLevel::NVLL_NONE, true);
        } catch (...) {
            return false;
        }

        NvComputer newState(http, serverInfo);

        // Ensure the machine that responded is the one we intended to contact
        if (m_Computer->uuid != newState.uuid) {
            qInfo() << "Found unexpected PC" << newState.name << "looking for" << m_Computer->name;
            return false;
        }

        changed = m_Computer->update(newState);
        return true;
    }

    bool updateAppList(QNetworkAccessManager* nam, bool& changed)
    {
        NvHTTP http(m_Computer, nam);

        QVector<NvApp> appList;

        try {
            appList = http.getAppList();
            if (appList.isEmpty()) {
                return false;
            }
        } catch (...) {
            return false;
        }

        QWriteLocker lock(&m_Computer->lock);
        changed = m_Computer->updateAppList(appList);
        return true;
    }

    void run() override
    {
        // Reduce the power and performance impact of our
        // computer status polling while it's running.
        setPriority(QThread::LowPriority);
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
        setServiceLevel(QThread::QualityOfService::Eco);
#endif

        // Share the QNetworkAccessManager to conserve resources when polling.
        // Each instance creates a worker thread, so sharing them ensures that
        // we are not spamming a new thread for every single polling attempt.
        //
        // Since QThread inherit the priority of the current thread, this also
        // ensures that the NAM's worker thread will inherit our lower priority.
        QNetworkAccessManager nam;

        // Always fetch the applist the first time
        int pollsSinceLastAppListFetch = POLLS_PER_APPLIST_FETCH;
        while (!isInterruptionRequested()) {
            bool stateChanged = false;
            bool online = false;
            bool wasOnline = m_Computer->state == NvComputer::CS_ONLINE;
            for (int i = 0; i < (wasOnline ? TRIES_BEFORE_OFFLINING : 1) && !online; i++) {
                for (auto& address : m_Computer->uniqueAddresses()) {
                    if (isInterruptionRequested()) {
                        return;
                    }

                    if (tryPollComputer(&nam, address, stateChanged)) {
                        if (!wasOnline) {
                            qInfo() << m_Computer->name << "is now online at" << m_Computer->activeAddress.toString();
                        }
                        online = true;
                        break;
                    }
                }
            }

            // Check if we failed after all retry attempts
            // Note: we don't need to acquire the read lock here,
            // because we're on the writing thread.
            if (!online && m_Computer->state != NvComputer::CS_OFFLINE) {
                qInfo() << m_Computer->name << "is now offline";
                m_Computer->state = NvComputer::CS_OFFLINE;
                stateChanged = true;
                m_Computer->activeReachability = NvComputer::RI_UNKNOWN;
            }

            // Refresh the cached path label (LAN/Tailnet/VPN) when the host
            // just came online or we've never classified the current path.
            if (online && (!wasOnline || m_Computer->activeReachability == NvComputer::RI_UNKNOWN)) {
                m_Computer->activeReachability = m_Computer->getActiveAddressReachability();
            }

            // Grab the applist if it's empty or it's been long enough that we need to refresh
            pollsSinceLastAppListFetch++;
            if (m_Computer->state == NvComputer::CS_ONLINE &&
                    m_Computer->pairState == NvComputer::PS_PAIRED &&
                    (m_Computer->appList.isEmpty() || pollsSinceLastAppListFetch >= POLLS_PER_APPLIST_FETCH)) {
                // Notify prior to the app list poll since it may take a while, and we don't
                // want to delay onlining of a machine, especially if we already have a cached list.
                if (stateChanged) {
                    emit computerStateChanged(m_Computer);
                    stateChanged = false;
                }

                if (updateAppList(&nam, stateChanged)) {
                    pollsSinceLastAppListFetch = 0;
                }
            }

            if (stateChanged) {
                // Tell anyone listening that we've changed state
                emit computerStateChanged(m_Computer);
            }

            // Wait a bit to poll again, but do it in 100 ms chunks
            // so we can be interrupted reasonably quickly.
            // FIXME: QWaitCondition would be better.
            for (int i = 0; i < 30 && !isInterruptionRequested(); i++) {
                if (m_ProbeNow.exchange(false)) {
                    // A "Re-check now" request cut the sleep short
                    break;
                }
                QThread::msleep(100);
            }
        }
    }

public:
    void
    requestImmediateProbe() override
    {
        m_ProbeNow = true;
    }

signals:
   void computerStateChanged(NvComputer* computer);

private:
    std::atomic<bool> m_ProbeNow;
    NvComputer* m_Computer;
};

ComputerManager::ComputerManager(StreamingPreferences* prefs)
    : m_Prefs(prefs),
      m_PollingRef(0),
      m_MdnsBrowser(nullptr),
      m_CompatFetcher(nullptr),
      m_DelayedFlushTimer(new QTimer(this)),
      m_NeedsDelayedFlush(false)
{
    SettingsDatabase* database = SettingsDatabase::get();
    const QString migrationMarker = QStringLiteral("migration.qsettings_hosts_imported_v1");
    const bool alreadyImported = database && database->isOpen()
            && database->setting(migrationMarker, false).toBool();

    if (!alreadyImported && database && database->isOpen()) {
        QSettings settings;

        // If there's a hosts backup copy, we must have failed to commit
        // a previous QSettings-era update before exiting. Prefer the
        // backup, matching the old flush thread's own recovery order.
        int hostCount = settings.beginReadArray(SER_HOSTS_BACKUP);
        if (hostCount == 0) {
            settings.endArray();
            hostCount = settings.beginReadArray(SER_HOSTS);
        }

        QVariantList legacyRecords;
        legacyRecords.reserve(hostCount);
        for (int i = 0; i < hostCount; i++) {
            settings.setArrayIndex(i);
            NvComputer legacyComputer(settings);
            legacyRecords.append(hostRecordFromComputer(&legacyComputer));
        }
        settings.endArray();

        if (!database->importLegacyHostRecords(legacyRecords, migrationMarker)) {
            qWarning() << "Failed to import legacy paired Hosts into the settings database:"
                       << database->lastError();
        }
        // The legacy QSettings hosts/hostsbackup arrays are left untouched
        // as rollback evidence -- the same policy every other QSettings ->
        // SQLite import in this codebase follows (StreamingPreferences,
        // ThemeManager, HostAdapterManager, ControllerProfileStore).
    }

    if (database && database->isOpen()) {
        const QVariantList records = database->hostRecords();
        for (const QVariant& recordVariant : records) {
            NvComputer* computer = computerFromHostRecord(recordVariant.toMap());
            m_KnownHosts[computer->uuid] = computer;
            m_LastSerializedHosts[computer->uuid] = *computer;
        }
    }
    else {
        qWarning() << "Settings database is unavailable; paired Hosts cannot be loaded or persisted this session";
    }

    // Fetch latest compatibility data asynchronously
    m_CompatFetcher.start();

    // SQLite writes must run on the thread that opened the Qt SQL
    // connection. Debounce Host snapshots on this manager's thread rather
    // than writing through the old QSettings worker thread.
    m_DelayedFlushTimer->setSingleShot(true);
    m_DelayedFlushTimer->setInterval(50);
    connect(m_DelayedFlushTimer, &QTimer::timeout,
            this, &ComputerManager::flushHosts);

    // To quit in a timely manner, we must block additional requests
    // after we receive the aboutToQuit() signal. This is necessary
    // because NvHTTP uses aboutToQuit() to abort requests in progress
    // while quitting, however this is a one time signal - additional
    // requests would not be aborted and block termination.
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, &ComputerManager::handleAboutToQuit);
}

ComputerManager::~ComputerManager()
{
    if (m_DelayedFlushTimer->isActive() || m_NeedsDelayedFlush) {
        m_DelayedFlushTimer->stop();
        flushHosts();
    }

    QWriteLocker lock(&m_Lock);

    // Delete machines that haven't been resolved yet
    while (!m_PendingResolution.isEmpty()) {
        MdnsPendingComputer* computer = m_PendingResolution.first();
        delete computer;
        m_PendingResolution.removeFirst();
    }

    // Delete the browser to stop discovery
    delete m_MdnsBrowser;
    m_MdnsBrowser = nullptr;

    // Interrupt polling
    for (ComputerPollingEntry* entry : std::as_const(m_PollEntries)) {
        entry->interrupt();
    }

    // Delete all polling entries (and associated threads)
    for (ComputerPollingEntry* entry : std::as_const(m_PollEntries)) {
        delete entry;
    }

    // Destroy all NvComputer objects now that polling is halted
    for (NvComputer* computer : std::as_const(m_KnownHosts)) {
        delete computer;
    }
}


void ComputerManager::saveHosts()
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this]() {
            saveHosts();
        }, Qt::QueuedConnection);
        return;
    }

    QMutexLocker locker(&m_DelayedFlushMutex);
    m_NeedsDelayedFlush = true;
    m_DelayedFlushTimer->start();
}

void ComputerManager::flushHosts()
{
    Q_ASSERT(QThread::currentThread() == thread());

    QVariantList records;
    QHash<QString, NvComputer> serialized;
    {
        QReadLocker lock(&m_Lock);
        records.reserve(m_KnownHosts.size());
        for (const NvComputer* computer : std::as_const(m_KnownHosts)) {
            QReadLocker computerLock(&computer->lock);
            records.append(hostRecordFromComputer(computer));
            serialized.insert(computer->uuid, *computer);
        }
    }

    SettingsDatabase* database = SettingsDatabase::get();
    if (database == nullptr || !database->isOpen()
            || !database->replaceHostRecords(records)) {
        qWarning() << "Failed to persist paired Hosts to the settings database:"
                   << (database ? database->lastError()
                                : QStringLiteral("database unavailable"));
        QMutexLocker locker(&m_DelayedFlushMutex);
        m_NeedsDelayedFlush = true;
        return;
    }

    QMutexLocker locker(&m_DelayedFlushMutex);
    m_LastSerializedHosts = std::move(serialized);
    m_NeedsDelayedFlush = false;
}

QHostAddress ComputerManager::getBestGlobalAddressV6(QVector<QHostAddress> &addresses)
{
    for (const QHostAddress& address : addresses) {
        if (address.protocol() == QAbstractSocket::IPv6Protocol) {
            if (address.isInSubnet(QHostAddress("fe80::"), 10)) {
                // Link-local
                continue;
            }

            if (address.isInSubnet(QHostAddress("fec0::"), 10)) {
                qInfo() << "Ignoring site-local address:" << address;
                continue;
            }

            if (address.isInSubnet(QHostAddress("fc00::"), 7)) {
                qInfo() << "Ignoring ULA:" << address;
                continue;
            }

            if (address.isInSubnet(QHostAddress("2002::"), 16)) {
                qInfo() << "Ignoring 6to4 address:" << address;
                continue;
            }

            if (address.isInSubnet(QHostAddress("2001::"), 32)) {
                qInfo() << "Ignoring Teredo address:" << address;
                continue;
            }

            return address;
        }
    }

    return QHostAddress();
}

void ComputerManager::startPolling()
{
    QWriteLocker lock(&m_Lock);

    if (++m_PollingRef > 1) {
        return;
    }

    if (m_Prefs->enableMdns) {
        // Start an MDNS query for GameStream hosts
        m_MdnsServer.reset(new QMdnsEngine::Server());
        m_MdnsBrowser = new QMdnsEngine::Browser(m_MdnsServer.data(), "_nvstream._tcp.local.");
        connect(m_MdnsBrowser, &QMdnsEngine::Browser::serviceAdded,
                this, [this](const QMdnsEngine::Service& service) {
            qInfo() << "Discovered mDNS host:" << service.hostname();

            MdnsPendingComputer* pendingComputer = new MdnsPendingComputer(m_MdnsServer, service);
            connect(pendingComputer, &MdnsPendingComputer::resolvedHost,
                    this, &ComputerManager::handleMdnsServiceResolved);
            m_PendingResolution.append(pendingComputer);
        });
    }
    else {
        qWarning() << "mDNS is disabled by user preference";
    }

    // Start polling threads for each known host
    QMapIterator<QString, NvComputer*> i(m_KnownHosts);
    while (i.hasNext()) {
        i.next();
        startPollingComputer(i.value());
    }
}

// Must hold m_Lock for write
void ComputerManager::startPollingComputer(NvComputer* computer)
{
    if (m_PollingRef == 0) {
        return;
    }

    ComputerPollingEntry* pollingEntry;

    if (!m_PollEntries.contains(computer->uuid)) {
        pollingEntry = m_PollEntries[computer->uuid] = new ComputerPollingEntry();
    }
    else {
        pollingEntry = m_PollEntries[computer->uuid];
    }

    if (!pollingEntry->isActive()) {
        PcMonitorThread* thread = new PcMonitorThread(computer);
        connect(thread, &PcMonitorThread::computerStateChanged,
                this, &ComputerManager::handleComputerStateChanged);
        pollingEntry->setActiveThread(thread);
        pollingEntry->setProber(thread);
        thread->start();
    }
}

void ComputerManager::handleMdnsServiceResolved(MdnsPendingComputer* computer,
                                                QVector<QHostAddress>& addresses)
{
    QHostAddress v6Global = getBestGlobalAddressV6(addresses);
    bool added = false;

    for (const QHostAddress& address : std::as_const(addresses)) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol) {
            // NB: We don't just call addNewHost() here with v6Global because the IPv6
            // address may not be reachable (if the user hasn't installed the IPv6 helper yet
            // or if this host lacks outbound IPv6 capability). We want to add IPv6 even if
            // it's not currently reachable.
            addNewHost(NvAddress(address, computer->port()),
                       true, computer->hostname(),
                       NvAddress(v6Global, computer->port()));
            added = true;
            break;
        }
    }

    if (!added) {
        // If we get here, there wasn't an IPv4 address so we'll do it v6-only
        for (const QHostAddress& address : std::as_const(addresses)) {
            if (address.protocol() == QAbstractSocket::IPv6Protocol) {
                // Use a link-local or site-local address for the "local address"
                if (address.isInSubnet(QHostAddress("fe80::"), 10) ||
                        address.isInSubnet(QHostAddress("fec0::"), 10) ||
                        address.isInSubnet(QHostAddress("fc00::"), 7)) {
                    addNewHost(NvAddress(address, computer->port()),
                               true, computer->hostname(),
                               NvAddress(v6Global, computer->port()));
                    break;
                }
            }
        }
    }

    m_PendingResolution.removeOne(computer);
    computer->deleteLater();
}

void ComputerManager::saveHost(NvComputer *computer)
{
    if (QThread::currentThread() != thread()) {
        saveHosts();
        return;
    }
    // If no serializable properties changed, don't bother saving hosts
    QMutexLocker lock(&m_DelayedFlushMutex);
    QReadLocker computerLock(&computer->lock);
    if (!m_LastSerializedHosts.value(computer->uuid).isEqualSerialized(*computer)) {
        // Queue a debounced SQLite transaction outside of the lock.
        computerLock.unlock();
        lock.unlock();
        saveHosts();
    }
}

void ComputerManager::handleComputerStateChanged(NvComputer* computer)
{
    emit computerStateChanged(computer);

    if (computer->pendingQuit && computer->currentGameId == 0) {
        computer->pendingQuit = false;
        emit quitAppCompleted(QVariant());
    }

    // Save updates to this host
    saveHost(computer);
}

QVector<NvComputer*> ComputerManager::getComputers()
{
    QReadLocker lock(&m_Lock);

    // Return a sorted host list
    auto hosts = QVector<NvComputer*>::fromList(m_KnownHosts.values());
    std::stable_sort(hosts.begin(), hosts.end(), [](const NvComputer* host1, const NvComputer* host2) {
        return host1->name.toLower() < host2->name.toLower();
    });
    return hosts;
}

class DeferredHostDeletionTask : public QRunnable
{
public:
    DeferredHostDeletionTask(ComputerManager* cm, NvComputer* computer)
        : m_Computer(computer),
          m_ComputerManager(cm) {}

    void run()
    {
        ComputerPollingEntry* pollingEntry;

        // Only do the minimum amount of work while holding the writer lock.
        // We must release it before calling saveHosts().
        {
            QWriteLocker lock(&m_ComputerManager->m_Lock);

            pollingEntry = m_ComputerManager->m_PollEntries.take(m_Computer->uuid);

            m_ComputerManager->m_KnownHosts.remove(m_Computer->uuid);
        }

        // Persist the new host list with this computer deleted
        m_ComputerManager->saveHosts();

        // Delete the polling entry first. This will stop all polling threads too.
        delete pollingEntry;

        // Delete cached box art
        BoxArtManager::deleteBoxArt(m_Computer);

        // Finally, delete the computer itself. This must be done
        // last because the polling thread might be using it.
        delete m_Computer;
    }

private:
    NvComputer* m_Computer;
    ComputerManager* m_ComputerManager;
};

void ComputerManager::deleteHost(NvComputer* computer)
{
    // Punt to a worker thread to avoid stalling the
    // UI while waiting for the polling thread to die
    QThreadPool::globalInstance()->start(new DeferredHostDeletionTask(this, computer));
}

void ComputerManager::renameHost(NvComputer* computer, QString name)
{
    {
        QWriteLocker lock(&computer->lock);

        computer->name = name;
        computer->hasCustomName = true;
    }

    // Notify the UI of the state change
    handleComputerStateChanged(computer);
}

void ComputerManager::clientSideAttributeUpdated(NvComputer* computer)
{
    // Notify the UI of the state change
    handleComputerStateChanged(computer);
}

void ComputerManager::setWakeOverrides(NvComputer* computer, QString macAddress, quint16 port, QString broadcastAddress)
{
    {
        QWriteLocker lock(&computer->lock);

        // Colon-hex form ("aa:bb:cc:dd:ee:ff"); anything unparsable is dropped,
        // an empty result means "fall back to the learned MAC".
        QByteArray parsedMac;
        for (const QString& octet : macAddress.split(':', Qt::SkipEmptyParts)) {
            bool ok;
            uint value = octet.trimmed().toUInt(&ok, 16);
            if (ok && value <= 0xFF) {
                parsedMac.append(static_cast<char>(value));
            }
        }
        if (parsedMac.size() != 6) {
            parsedMac.clear();
        }

        computer->manualMacAddress = parsedMac;
        computer->wakePort = port;
        computer->wakeBroadcastAddress = broadcastAddress.trimmed();
    }

    // Persist immediately; these are explicit user edits, not polled state
    saveHosts();

    // Notify the UI of the state change
    handleComputerStateChanged(computer);
}

void ComputerManager::reprobeHost(NvComputer* computer)
{
    QReadLocker lock(&m_Lock);

    ComputerPollingEntry* entry = m_PollEntries.value(computer->uuid, nullptr);
    if (entry != nullptr) {
        entry->requestImmediateProbe();
    }
}

void ComputerManager::handleAboutToQuit()
{
    QReadLocker lock(&m_Lock);

    // Interrupt polling threads immediately, so they
    // avoid making additional requests while quitting
    for (ComputerPollingEntry* entry : std::as_const(m_PollEntries)) {
        entry->interrupt();
    }
}

class PendingPairingTask : public QObject, public QRunnable
{
    Q_OBJECT

public:
    PendingPairingTask(ComputerManager* computerManager, NvComputer* computer, QString pin)
        : m_ComputerManager(computerManager),
          m_Computer(computer),
          m_Pin(pin)
    {
        connect(this, &PendingPairingTask::pairingCompleted,
                computerManager, &ComputerManager::pairingCompleted);
    }

signals:
    void pairingCompleted(NvComputer* computer, QString error);

private:
    void run()
    {
        NvPairingManager pairingManager(m_Computer);

        try {
           NvPairingManager::PairState result = pairingManager.pair(m_Computer->appVersion, m_Pin, m_Computer->serverCert);
           switch (result)
           {
           case NvPairingManager::PairState::PIN_WRONG:
               emit pairingCompleted(m_Computer, tr("The PIN from the PC didn't match. Please try again."));
               break;
           case NvPairingManager::PairState::FAILED:
               if (m_Computer->currentGameId != 0) {
                   emit pairingCompleted(m_Computer, tr("You cannot pair while a previous session is still running on the host PC. Quit any running games or reboot the host PC, then try pairing again."));
               }
               else {
                   emit pairingCompleted(m_Computer, tr("Pairing failed. Please try again."));
               }
               break;
           case NvPairingManager::PairState::ALREADY_IN_PROGRESS:
               emit pairingCompleted(m_Computer, tr("Another pairing attempt is already in progress."));
               break;
           case NvPairingManager::PairState::PAIRED:
               // Deliberate, user-initiated re-pair is the only path that
               // may clear Identity Changed (ADR-0007) -- pair() already
               // wrote the newly pinned certificate into m_Computer->serverCert
               // above; adopt it as trusted and drop the stale pending one.
               m_Computer->identityChanged = false;
               m_Computer->pendingServerCert = QSslCertificate();

               // Persist the newly pinned server certificate for this host
               m_ComputerManager->saveHost(m_Computer);

               emit pairingCompleted(m_Computer, nullptr);
               break;
           }
        } catch (const GfeHttpResponseException& e) {
            emit pairingCompleted(m_Computer, tr("GeForce Experience returned error: %1").arg(e.toQString()));
        } catch (const QtNetworkReplyException& e) {
            emit pairingCompleted(m_Computer, e.toQString());
        }
    }

    ComputerManager* m_ComputerManager;
    NvComputer* m_Computer;
    QString m_Pin;
};

void ComputerManager::pairHost(NvComputer* computer, QString pin)
{
    // Punt to a worker thread to avoid stalling the
    // UI while waiting for pairing to complete
    PendingPairingTask* pairing = new PendingPairingTask(this, computer, pin);
    QThreadPool::globalInstance()->start(pairing);
}

class PendingQuitTask : public QObject, public QRunnable
{
    Q_OBJECT

public:
    PendingQuitTask(ComputerManager* computerManager, NvComputer* computer)
        : m_Computer(computer)
    {
        connect(this, &PendingQuitTask::quitAppFailed,
                computerManager, &ComputerManager::quitAppCompleted);
    }

signals:
    void quitAppFailed(QString error);

private:
    void run()
    {
        NvHTTP http(m_Computer);

        try {
            if (m_Computer->currentGameId != 0) {
                http.quitApp();
            }
        } catch (const GfeHttpResponseException& e) {
            {
                QWriteLocker lock(&m_Computer->lock);
                m_Computer->pendingQuit = false;
            }
            if (e.getStatusCode() == 599) {
                // 599 is a special code we make a custom message for
                emit quitAppFailed(tr("The running game wasn't started by this PC. "
                                      "You must quit the game on the host PC manually or use the device that originally started the game."));
            }
            else {
                emit quitAppFailed(e.toQString());
            }
        } catch (const QtNetworkReplyException& e) {
            {
                QWriteLocker lock(&m_Computer->lock);
                m_Computer->pendingQuit = false;
            }
            emit quitAppFailed(e.toQString());
        }
    }

    NvComputer* m_Computer;
};

void ComputerManager::quitRunningApp(NvComputer* computer)
{
    QWriteLocker lock(&computer->lock);
    computer->pendingQuit = true;

    PendingQuitTask* quit = new PendingQuitTask(this, computer);
    QThreadPool::globalInstance()->start(quit);
}

void ComputerManager::stopPollingAsync()
{
    QWriteLocker lock(&m_Lock);

    Q_ASSERT(m_PollingRef > 0);
    if (--m_PollingRef > 0) {
        return;
    }

    // Delete machines that haven't been resolved yet
    while (!m_PendingResolution.isEmpty()) {
        MdnsPendingComputer* computer = m_PendingResolution.first();
        computer->deleteLater();
        m_PendingResolution.removeFirst();
    }

    // Delete the browser and server to stop discovery and refresh polling
    delete m_MdnsBrowser;
    m_MdnsBrowser = nullptr;
    m_MdnsServer.reset();

    // Interrupt all threads, but don't wait for them to terminate
    for (ComputerPollingEntry* entry : std::as_const(m_PollEntries)) {
        entry->interrupt();
    }
}

void ComputerManager::addNewHostManually(QString address)
{
    QUrl url = QUrl::fromUserInput("moonlight://" + address);
    if (url.isValid() && !url.host().isEmpty() && url.scheme() == "moonlight") {
        // If there wasn't a port specified, use the default
        addNewHost(NvAddress(url.host(), url.port(DEFAULT_HTTP_PORT)), false);
    }
    else if (QHostAddress(address).protocol() == QAbstractSocket::IPv6Protocol) {
        // The user specified an IPv6 literal without URL escaping, so use the default port
        addNewHost(NvAddress(address, DEFAULT_HTTP_PORT), false);
    }
    else {
        emit computerAddCompleted(false, false);
    }
}

class PendingAddTask : public QObject, public QRunnable
{
    Q_OBJECT

public:
    PendingAddTask(ComputerManager* computerManager, QString name, NvAddress address, NvAddress mdnsIpv6Address, bool mdns)
        : m_ComputerManager(computerManager),
          m_Name(name),
          m_Address(address),
          m_MdnsIpv6Address(mdnsIpv6Address),
          m_Mdns(mdns),
          m_AboutToQuit(false)
    {
        connect(this, &PendingAddTask::computerAddCompleted,
                computerManager, &ComputerManager::computerAddCompleted);
        connect(this, &PendingAddTask::computerStateChanged,
                computerManager, &ComputerManager::handleComputerStateChanged);
        connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
                this, &PendingAddTask::handleAboutToQuit);
    }

signals:
    void computerAddCompleted(QVariant success, QVariant detectedPortBlocking);

    void computerStateChanged(NvComputer* computer);

private:
    void handleAboutToQuit()
    {
        m_AboutToQuit = true;
    }

    QString fetchServerInfo(NvHTTP& http)
    {
        QString serverInfo;

        // Do nothing if we're quitting
        if (m_AboutToQuit) {
            return QString();
        }

        try {
            // There's a race condition between GameStream servers reporting presence over
            // mDNS and the HTTPS server being ready to respond to our queries. To work
            // around this issue, we will issue the request again after a few seconds if
            // we see a ServiceUnavailableError error.
            try {
                serverInfo = http.getServerInfo(NvHTTP::NVLL_VERBOSE);
            } catch (const QtNetworkReplyException& e) {
                if (e.getError() == QNetworkReply::ServiceUnavailableError) {
                    qWarning() << "Retrying request in 5 seconds after ServiceUnavailableError";
                    QThread::sleep(5);
                    serverInfo = http.getServerInfo(NvHTTP::NVLL_VERBOSE);
                    qInfo() << "Retry successful";
                }
                else {
                    // Rethrow other errors
                    throw e;
                }
            }
            return serverInfo;
        } catch (...) {
            if (!m_Mdns) {
                unsigned int portTestResult;

                if (m_ComputerManager->m_Prefs->detectNetworkBlocking) {
                    // We failed to connect to the specified PC. Let's test to make sure this network
                    // isn't blocking Moonlight, so we can tell the user about it.
                    portTestResult = LiTestClientConnectivity("qt.conntest.moonlight-stream.org", 443,
                                                              ML_PORT_FLAG_TCP_47984 | ML_PORT_FLAG_TCP_47989);
                }
                else {
                    portTestResult = 0;
                }

                emit computerAddCompleted(false, portTestResult != 0 && portTestResult != ML_TEST_RESULT_INCONCLUSIVE);
            }
            return QString();
        }
    }

    void run()
    {
        // Use the placeholder UID for the initial poll, then we'll switch to the real one if it's not GFE
        NvHTTP http(m_Address, 0, QSslCertificate(), false);

        if (m_Mdns) {
            if (m_MdnsIpv6Address.isNull()) {
                qInfo() << "Processing new PC" << m_Name << "from mDNS with local address" << m_Address.toString();
            }
            else {
                qInfo() << "Processing new PC" << m_Name << "from mDNS with local address" << m_Address.toString()
                        << "and IPv6 address" << m_MdnsIpv6Address.toString();
            }
        }
        else {
            qInfo() << "Processing new PC at" << m_Address.toString() << "from user";
        }

        // Perform initial serverinfo fetch over HTTP since we don't know which cert to use
        QString serverInfo = fetchServerInfo(http);
        if (serverInfo.isEmpty() && !m_MdnsIpv6Address.isNull()) {
            // Retry using the global IPv6 address if the IPv4 or link-local IPv6 address fails
            http.setAddress(m_MdnsIpv6Address);
            serverInfo = fetchServerInfo(http);
        }
        if (serverInfo.isEmpty()) {
            return;
        }

        // Create initial newComputer using HTTP serverinfo with no pinned cert
        NvComputer* newComputer = new NvComputer(http, serverInfo);
        http.setTrueUid(!newComputer->isNvidiaServerSoftware);

        // Check if we have a record of this host UUID to pull the pinned cert
        NvComputer* existingComputer;
        {
            QReadLocker lock(&m_ComputerManager->m_Lock);
            existingComputer = m_ComputerManager->m_KnownHosts.value(newComputer->uuid);
            if (existingComputer != nullptr) {
                http.setServerCert(existingComputer->serverCert);
            }
        }

        // Fetch serverinfo again over HTTPS with the pinned cert
        if (existingComputer != nullptr) {
            Q_ASSERT(http.httpsPort() != 0);
            serverInfo = fetchServerInfo(http);
            if (serverInfo.isEmpty()) {
                return;
            }

            // Update the polled computer with the HTTPS information
            NvComputer httpsComputer(http, serverInfo);
            newComputer->update(httpsComputer);
        }

        // Update addresses depending on the context
        if (m_Mdns) {
            // Only update local address if we actually reached the PC via this address.
            // If we reached it via the IPv6 address after the local address failed,
            // don't store the non-working local address.
            if (http.address() == m_Address) {
                newComputer->localAddress = m_Address;
            }

            // Get the WAN IP address using STUN if we're on mDNS over IPv4
            if (QHostAddress(newComputer->localAddress.address()).protocol() == QAbstractSocket::IPv4Protocol) {
                quint32 addr;
                int err = LiFindExternalAddressIP4("stun.moonlight-stream.org", 3478, &addr);
                if (err == 0) {
                    newComputer->setRemoteAddress(QHostAddress(qFromBigEndian(addr)));
                }
                else {
                    qWarning() << "STUN failed to get WAN address:" << err;
                }
            }

            if (!m_MdnsIpv6Address.isNull()) {
                Q_ASSERT(QHostAddress(m_MdnsIpv6Address.address()).protocol() == QAbstractSocket::IPv6Protocol);
                newComputer->ipv6Address = m_MdnsIpv6Address;
            }
        }
        else {
            newComputer->manualAddress = m_Address;
        }

        QHostAddress hostAddress(m_Address.address());
        bool addressIsSiteLocalV4 =
                hostAddress.isInSubnet(QHostAddress("10.0.0.0"), 8) ||
                hostAddress.isInSubnet(QHostAddress("172.16.0.0"), 12) ||
                hostAddress.isInSubnet(QHostAddress("192.168.0.0"), 16);

        {
            // Check if this PC already exists using opportunistic read lock
            m_ComputerManager->m_Lock.lockForRead();
            NvComputer* existingComputer = m_ComputerManager->m_KnownHosts.value(newComputer->uuid);

            // If it doesn't already exist, convert to a write lock in preparation for updating.
            //
            // NB: ComputerManager's lock protects the host map itself, not the elements inside.
            // Those are protected by their individual locks. Since we only mutate the map itself
            // when the PC doesn't exist, we need the lock in write-mode for that case only.
            if (existingComputer == nullptr) {
                m_ComputerManager->m_Lock.unlock();
                m_ComputerManager->m_Lock.lockForWrite();

                // Since we had to unlock to lock for write, someone could have raced and added
                // this PC before us. We have to check again whether it already exists.
                existingComputer = m_ComputerManager->m_KnownHosts.value(newComputer->uuid);
            }

            if (existingComputer != nullptr) {
                // Fold it into the existing PC
                bool changed = existingComputer->update(*newComputer);
                delete newComputer;

                // Drop the lock before notifying
                m_ComputerManager->m_Lock.unlock();

                // For non-mDNS clients, let them know it succeeded
                if (!m_Mdns) {
                    emit computerAddCompleted(true, false);
                }

                // Tell our client if something changed
                if (changed) {
                    qInfo() << existingComputer->name << "is now at" << existingComputer->activeAddress.toString();
                    emit computerStateChanged(existingComputer);
                }
            }
            else {
                // Store this in our active sets
                m_ComputerManager->m_KnownHosts[newComputer->uuid] = newComputer;

                // Start polling if enabled (write lock required)
                m_ComputerManager->startPollingComputer(newComputer);

                // Drop the lock before notifying
                m_ComputerManager->m_Lock.unlock();

                // If this wasn't added via mDNS but it is a RFC 1918 IPv4 address and not a VPN,
                // go ahead and do the STUN request now to populate an external address.
                if (!m_Mdns && addressIsSiteLocalV4 && newComputer->getActiveAddressReachability() != NvComputer::RI_VPN &&
                        newComputer->getActiveAddressReachability() != NvComputer::RI_TAILNET) {
                    quint32 addr;
                    int err = LiFindExternalAddressIP4("stun.moonlight-stream.org", 3478, &addr);
                    if (err == 0) {
                        newComputer->setRemoteAddress(QHostAddress(qFromBigEndian(addr)));
                    }
                    else {
                        qWarning() << "STUN failed to get WAN address:" << err;
                    }
                }

                // For non-mDNS clients, let them know it succeeded
                if (!m_Mdns) {
                    emit computerAddCompleted(true, false);
                }

                // Tell our client about this new PC
                emit computerStateChanged(newComputer);
            }
        }
    }

    ComputerManager* m_ComputerManager;
    QString m_Name;
    NvAddress m_Address;
    NvAddress m_MdnsIpv6Address;
    bool m_Mdns;
    bool m_AboutToQuit;
};

void ComputerManager::addNewHost(NvAddress address, bool mdns, QString name, NvAddress mdnsIpv6Address)
{
    // Punt to a worker thread to avoid stalling the
    // UI while waiting for serverinfo query to complete
    PendingAddTask* addTask = new PendingAddTask(this, name, address, mdnsIpv6Address, mdns);
    QThreadPool::globalInstance()->start(addTask);
}

QString ComputerManager::generatePinString()
{
    return QString::asprintf("%04u", QRandomGenerator::system()->bounded(10000));
}

#include "computermanager.moc"
