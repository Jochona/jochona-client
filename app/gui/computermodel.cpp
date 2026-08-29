#include "computermodel.h"
#include "settings/effectivesettingsresolver.h"
#include "library/librarymanager.h"
#include "backend/adapters/hostadaptermanager.h"
#include "backend/wake/wakeprovider.h"

#include <QThreadPool>
#include <QStringList>
#include <QSharedPointer>

#include <Limelight.h> // SCM_* codec bits

ComputerModel::ComputerModel(QObject* object)
    : QAbstractListModel(object) {}

void ComputerModel::initialize(ComputerManager* computerManager)
{
    m_ComputerManager = computerManager;
    connect(m_ComputerManager, &ComputerManager::computerStateChanged,
            this, &ComputerModel::handleComputerStateChanged);
    connect(m_ComputerManager, &ComputerManager::pairingCompleted,
            this, &ComputerModel::handlePairingCompleted);

    m_Computers = m_ComputerManager->getComputers();
}

QVariant ComputerModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    Q_ASSERT(index.row() < m_Computers.count());

    NvComputer* computer = m_Computers[index.row()];
    QReadLocker lock(&computer->lock);

    switch (role) {
    case NameRole:
        return computer->name;
    case OnlineRole:
        return computer->state == NvComputer::CS_ONLINE;
    case PairedRole:
        return computer->pairState == NvComputer::PS_PAIRED;
    case BusyRole:
        return computer->currentGameId != 0;
    case WakeableRole:
        return WakeProviderManager::canWake(*computer);
    case StatusUnknownRole:
        return computer->state == NvComputer::CS_UNKNOWN;
    case ServerSupportedRole:
        return computer->isSupportedServerVersion;
    case WakePortRole:
        return computer->wakePort;
    case WakeBroadcastRole:
        return computer->wakeBroadcastAddress;
    case ManualMacRole:
        return computer->manualMacAddress.isEmpty() ?
                   QString() : QString(computer->manualMacAddress.toHex(':'));
    case ConnectionPathRole:
        switch (computer->activeReachability) {
        case NvComputer::RI_LAN:
            return tr("Direct (LAN)");
        case NvComputer::RI_VPN:
            return tr("VPN");
        case NvComputer::RI_TAILNET:
            return tr("Tailnet");
        default:
            return tr("Unknown");
        }
    case DetailsRole: {
        QString state, pairState;

        switch (computer->state) {
        case NvComputer::CS_ONLINE:
            state = tr("Online");
            break;
        case NvComputer::CS_OFFLINE:
            state = tr("Offline");
            break;
        default:
            state = tr("Unknown");
            break;
        }

        switch (computer->pairState) {
        case NvComputer::PS_PAIRED:
            pairState = tr("Paired");
            break;
        case NvComputer::PS_NOT_PAIRED:
            pairState = tr("Unpaired");
            break;
        default:
            pairState = tr("Unknown");
            break;
        }

        return tr("Name: %1").arg(computer->name) + '\n' +
               tr("Status: %1").arg(state) + '\n' +
               tr("Active Address: %1").arg(computer->activeAddress.toString()) + '\n' +
               tr("UUID: %1").arg(computer->uuid) + '\n' +
               tr("Local Address: %1").arg(computer->localAddress.toString()) + '\n' +
               tr("Remote Address: %1").arg(computer->remoteAddress.toString()) + '\n' +
               tr("IPv6 Address: %1").arg(computer->ipv6Address.toString()) + '\n' +
               tr("Manual Address: %1").arg(computer->manualAddress.toString()) + '\n' +
               tr("MAC Address: %1").arg(computer->macAddress.isEmpty() ? tr("Unknown") : QString(computer->macAddress.toHex(':'))) + '\n' +
               tr("Manual MAC: %1").arg(computer->manualMacAddress.isEmpty() ? tr("None") : QString(computer->manualMacAddress.toHex(':'))) + '\n' +
               tr("Wake Port: %1").arg(computer->wakePort == 0 ? tr("Automatic") : QString::number(computer->wakePort)) + '\n' +
               tr("Wake Broadcast: %1").arg(computer->wakeBroadcastAddress.isEmpty() ? tr("All interfaces") : computer->wakeBroadcastAddress) + '\n' +
               tr("Pair State: %1").arg(pairState) + '\n' +
               tr("Running Game ID: %1").arg(computer->state == NvComputer::CS_ONLINE ? QString::number(computer->currentGameId) : tr("Unknown")) + '\n' +
               tr("HTTPS Port: %1").arg(computer->state == NvComputer::CS_ONLINE ? QString::number(computer->activeHttpsPort) : tr("Unknown"));
    }
    case UuidRole:
        return computer->uuid;
    case IndexRole:
        return index.row();
    case WakeStateRole:
        return m_WakeOperations.value(computer->uuid).state;
    case WakeProviderRole:
        return WakeProviderManager::providerName(computer->uuid);
    case WakeErrorRole:
        return m_WakeOperations.value(computer->uuid).error;
    default:
        return QVariant();
    }
}

int ComputerModel::rowCount(const QModelIndex& parent) const
{
    // We should not return a count for valid index values,
    // only the parent (which will not have a "valid" index).
    if (parent.isValid()) {
        return 0;
    }

    return m_Computers.count();
}

QHash<int, QByteArray> ComputerModel::roleNames() const
{
    QHash<int, QByteArray> names;

    names[NameRole] = "name";
    names[OnlineRole] = "online";
    names[PairedRole] = "paired";
    names[BusyRole] = "busy";
    names[WakeableRole] = "wakeable";
    names[StatusUnknownRole] = "statusUnknown";
    names[ServerSupportedRole] = "serverSupported";
    names[DetailsRole] = "details";
    names[WakePortRole] = "wakePort";
    names[WakeBroadcastRole] = "wakeBroadcast";
    names[ManualMacRole] = "manualMac";
    names[ConnectionPathRole] = "connectionPath";
    names[UuidRole] = "uuid";
    names[IndexRole] = "index";
    names[WakeStateRole] = "wakeState";
    names[WakeProviderRole] = "wakeProvider";
    names[WakeErrorRole] = "wakeError";

    return names;
}

Session* ComputerModel::createSessionForCurrentGame(int computerIndex)
{
    Q_ASSERT(computerIndex < m_Computers.count());

    NvComputer* computer = m_Computers[computerIndex];

    // We must currently be streaming a game to use this function
    Q_ASSERT(computer->currentGameId != 0);

    for (NvApp& app : computer->appList) {
        if (app.id == computer->currentGameId) {
            const QVariantMap context{
                {QStringLiteral("hostUuid"), computer->uuid},
                {QStringLiteral("libraryEntryId"),
                 LibraryManager::get()->libraryEntryFor(
                     computer->uuid, app.id)},
                {QStringLiteral("appId"), app.id},
            };
            StreamingPreferences* resolved =
                    EffectiveSettingsResolver::get()->createPreferences(context);
            Session* session = new Session(computer, app, resolved);
            delete resolved;
            return session;
        }
    }

    // We have a current running app but it's not in our app list
    Q_ASSERT(false);
    return nullptr;
}

int ComputerModel::indexOfUuid(const QString& uuid) const
{
    for (int i = 0; i < m_Computers.count(); i++) {
        QReadLocker lock(&m_Computers[i]->lock);
        if (m_Computers[i]->uuid == uuid) {
            return i;
        }
    }
    return -1;
}

bool ComputerModel::isOnlinePaired(int computerIndex) const
{
    if (computerIndex < 0 || computerIndex >= m_Computers.count()) {
        return false;
    }

    NvComputer* computer = m_Computers[computerIndex];
    QReadLocker lock(&computer->lock);
    return computer->state == NvComputer::CS_ONLINE &&
           computer->pairState == NvComputer::PS_PAIRED;
}

QString ComputerModel::nameForIndex(int computerIndex) const
{
    if (computerIndex < 0 || computerIndex >= m_Computers.count()) {
        return QString();
    }

    NvComputer* computer = m_Computers[computerIndex];
    QReadLocker lock(&computer->lock);
    return computer->name;
}

QString ComputerModel::uuidForIndex(int computerIndex) const
{
    if (computerIndex < 0 || computerIndex >= m_Computers.count()) {
        return QString();
    }

    NvComputer* computer = m_Computers[computerIndex];
    QReadLocker lock(&computer->lock);
    return computer->uuid;
}

QVariantMap ComputerModel::hostInfoForIndex(int computerIndex) const
{
    QVariantMap info;
    if (computerIndex < 0 || computerIndex >= m_Computers.count()) {
        info.insert("online", false);
        info.insert("statusUnknown", true);
        info.insert("summary", tr("Unknown"));
        return info;
    }

    NvComputer* computer = m_Computers[computerIndex];
    QReadLocker lock(&computer->lock);

    const bool online = computer->state == NvComputer::CS_ONLINE;
    info.insert("name", computer->name);
    info.insert("online", online);
    info.insert("paired", computer->pairState == NvComputer::PS_PAIRED);
    info.insert("statusUnknown", computer->state == NvComputer::CS_UNKNOWN);
    const WakeOperation wake = m_WakeOperations.value(computer->uuid);
    info.insert("wakeable", WakeProviderManager::canWake(*computer));
    info.insert("canDirectWake",
                WakeProviderManager::canDirectWake(*computer));
    info.insert("wakeProvider",
                WakeProviderManager::providerName(computer->uuid));
    info.insert("wakeState", wake.state);
    info.insert("wakeError", wake.error);

    QStringList summaryParts;
    if (computer->state == NvComputer::CS_ONLINE) {
        summaryParts << tr("Online");
    } else if (computer->state == NvComputer::CS_OFFLINE) {
        summaryParts << tr("Offline");
    } else {
        summaryParts << tr("Checking…");
    }
    if (!computer->appVersion.isEmpty()) {
        summaryParts << computer->appVersion;
    }
    if (online) {
        if (computer->serverCodecModeSupport & SCM_AV1_MAIN8) {
            summaryParts << "AV1";
        } else if (computer->serverCodecModeSupport & (SCM_HEVC | SCM_HEVC_MAIN10)) {
            summaryParts << "HEVC";
        } else {
            summaryParts << "H.264";
        }
    }
    info.insert("summary", summaryParts.join(" · "));

    switch (computer->activeReachability) {
    case NvComputer::RI_LAN:
        info.insert("path", tr("Direct (LAN)"));
        break;
    case NvComputer::RI_VPN:
        info.insert("path", tr("VPN"));
        break;
    case NvComputer::RI_TAILNET:
        info.insert("path", tr("Tailnet"));
        break;
    default:
        info.insert("path", QString());
        break;
    }
    return info;
}

void ComputerModel::deleteComputer(int computerIndex)
{
    Q_ASSERT(computerIndex < m_Computers.count());
    const QString uuid = uuidForIndex(computerIndex);
    auto wake = m_WakeOperations.find(uuid);
    if (wake != m_WakeOperations.end() && wake->probeTimer) {
        wake->probeTimer->stop();
        wake->probeTimer->deleteLater();
    }
    m_WakeOperations.remove(uuid);

    beginRemoveRows(QModelIndex(), computerIndex, computerIndex);

    // m_Computer[computerIndex] will be deleted by this call
    m_ComputerManager->deleteHost(m_Computers[computerIndex]);

    // Remove the now invalid item
    m_Computers.removeAt(computerIndex);

    endRemoveRows();
}

class WakeBurstTask : public QObject, public QRunnable
{
    Q_OBJECT

public:
    WakeBurstTask(QSharedPointer<NvComputer> computer,
                  QString uuid,
                  int generation,
                  bool forceDirect)
        : m_Computer(std::move(computer))
        , m_Uuid(std::move(uuid))
        , m_Generation(generation)
        , m_ForceDirect(forceDirect)
    {
    }

    void run() override
    {
        QString receipt;
        QString error;
        const bool sent = m_ForceDirect
            ? WakeProviderManager::sendDirect(*m_Computer, &receipt, &error)
            : WakeProviderManager::send(*m_Computer, &receipt, &error);
        emit completed(m_Uuid, m_Generation, sent, error);
    }

signals:
    void completed(QString uuid, int generation, bool sent, QString error);

private:
    QSharedPointer<NvComputer> m_Computer;
    QString m_Uuid;
    int m_Generation;
    bool m_ForceDirect;
};

bool ComputerModel::wakeComputer(int computerIndex,
                                 bool continueWhenReady,
                                 int appId,
                                 bool forceDirect)
{
    if (computerIndex < 0 || computerIndex >= m_Computers.count()) {
        return false;
    }
    NvComputer* computer = m_Computers[computerIndex];
    QString uuid;
    bool online;
    bool wakeable;
    QSharedPointer<NvComputer> wakeSnapshot;
    {
        QReadLocker lock(&computer->lock);
        uuid = computer->uuid;
        online = computer->state == NvComputer::CS_ONLINE;
        wakeable = forceDirect
            ? WakeProviderManager::canDirectWake(*computer)
            : WakeProviderManager::canWake(*computer);
        wakeSnapshot.reset(new NvComputer(*computer));
    }

    const QVector<int> dispatchDelays = forceDirect
        ? WakeProviderManager::directDispatchDelaysMs()
        : WakeProviderManager::clientDispatchDelaysMs(uuid);
    WakeOperation previous = m_WakeOperations.value(uuid);
    if (previous.probeTimer) {
        previous.probeTimer->stop();
        previous.probeTimer->deleteLater();
    }
    WakeOperation operation;
    operation.generation = previous.generation + 1;
    operation.attemptsExpected = dispatchDelays.size();
    operation.continueWhenReady = continueWhenReady;
    operation.appId = appId;
    operation.providerName = forceDirect
        ? tr("Direct Wake")
        : WakeProviderManager::providerName(uuid);
    m_WakeOperations.insert(uuid, operation);
    if (!online && (!wakeable || dispatchDelays.isEmpty())) {
        m_WakeOperations[uuid].error =
            tr("%1 is not configured for this Host.")
                .arg(operation.providerName);
        setWakeState(uuid, QStringLiteral("failed"));
        return false;
    }
    setWakeState(uuid, QStringLiteral("sending"));

    if (online) {
        finishWake(uuid, QStringLiteral("ready"));
        return true;
    }

    const int generation = operation.generation;
    for (int delay : dispatchDelays) {
        QTimer::singleShot(delay, this,
                           [this, uuid, generation, wakeSnapshot, forceDirect]() {
            auto it = m_WakeOperations.find(uuid);
            if (it == m_WakeOperations.end()
                    || it->generation != generation
                    || it->state == QStringLiteral("failed")
                    || it->state == QStringLiteral("ready")) {
                return;
            }
            auto* task = new WakeBurstTask(
                wakeSnapshot, uuid, generation, forceDirect);
            connect(task, &WakeBurstTask::completed,
                    this, &ComputerModel::handleWakeAttempt);
            QThreadPool::globalInstance()->start(task);
        });
    }

    QTimer* probeTimer = new QTimer(this);
    probeTimer->setInterval(1500);
    connect(probeTimer, &QTimer::timeout, this,
            [this, uuid, generation]() {
        auto it = m_WakeOperations.find(uuid);
        if (it == m_WakeOperations.end()
                || it->generation != generation) {
            return;
        }
        const int index = indexOfUuid(uuid);
        if (index >= 0) m_ComputerManager->reprobeHost(m_Computers[index]);
    });
    m_WakeOperations[uuid].probeTimer = probeTimer;
    probeTimer->start();

    QTimer::singleShot(60000, this, [this, uuid, generation]() {
        auto it = m_WakeOperations.find(uuid);
        if (it != m_WakeOperations.end()
                && it->generation == generation
                && it->state != QStringLiteral("ready")) {
            if (it->error.isEmpty()) {
                it->error =
                    tr("The Host did not become ready within one minute.");
            }
            finishWake(uuid, QStringLiteral("failed"));
        }
    });
    return true;
}

void ComputerModel::handleWakeAttempt(const QString& uuid,
                                      int generation,
                                      bool sent,
                                      QString error)
{
    auto it = m_WakeOperations.find(uuid);
    if (it == m_WakeOperations.end() || it->generation != generation
            || it->state == QStringLiteral("ready")
            || it->state == QStringLiteral("failed")) {
        return;
    }
    ++it->attemptsFinished;
    it->anySent = it->anySent || sent;
    if (sent) {
        it->error.clear();
        if (it->state == QStringLiteral("sending")) {
            setWakeState(uuid, QStringLiteral("sent"));
        }
    } else if (!error.isEmpty()) {
        it->error = tr("%1: %2").arg(it->providerName, error);
    }
    const int index = indexOfUuid(uuid);
    if (index >= 0) m_ComputerManager->reprobeHost(m_Computers[index]);
    if (it->attemptsFinished == it->attemptsExpected && !it->anySent) {
        finishWake(uuid, QStringLiteral("failed"));
    }
}

void ComputerModel::setWakeState(const QString& uuid,
                                 const QString& state)
{
    auto it = m_WakeOperations.find(uuid);
    if (it == m_WakeOperations.end() || it->state == state) return;
    it->state = state;
    const int index = indexOfUuid(uuid);
    if (index < 0) return;
    const QModelIndex changed = createIndex(index, 0);
    emit dataChanged(changed, changed, { WakeStateRole, WakeErrorRole });
    emit wakeStateChanged(index, state);
}

void ComputerModel::finishWake(const QString& uuid,
                               const QString& state)
{
    auto it = m_WakeOperations.find(uuid);
    if (it == m_WakeOperations.end()) return;
    if (it->probeTimer) {
        it->probeTimer->stop();
        it->probeTimer->deleteLater();
        it->probeTimer = nullptr;
    }
    const bool continueWhenReady =
        state == QStringLiteral("ready") && it->continueWhenReady;
    const int appId = it->appId;
    it->continueWhenReady = false;
    if (state == QLatin1String("ready")) {
        it->error.clear();
    }
    setWakeState(uuid, state);
    if (continueWhenReady) {
        const int index = indexOfUuid(uuid);
        if (index >= 0) emit wakeReady(index, appId);
    }
}

void ComputerModel::renameComputer(int computerIndex, QString name)
{
    Q_ASSERT(computerIndex < m_Computers.count());

    m_ComputerManager->renameHost(m_Computers[computerIndex], name);
}

void ComputerModel::setWakeOverrides(int computerIndex, QString mac, int port, QString broadcast)
{
    Q_ASSERT(computerIndex < m_Computers.count());

    m_ComputerManager->setWakeOverrides(m_Computers[computerIndex], mac,
                                        static_cast<quint16>(qBound(0, port, 65535)),
                                        broadcast);
}

void ComputerModel::reprobeComputer(int computerIndex)
{
    Q_ASSERT(computerIndex < m_Computers.count());

    m_ComputerManager->reprobeHost(m_Computers[computerIndex]);
}

QString ComputerModel::generatePinString()
{
    return m_ComputerManager->generatePinString();
}

class DeferredTestConnectionTask : public QObject, public QRunnable
{
    Q_OBJECT
public:
    void run()
    {
        unsigned int portTestResult = LiTestClientConnectivity("qt.conntest.moonlight-stream.org", 443, ML_PORT_FLAG_ALL);
        if (portTestResult == ML_TEST_RESULT_INCONCLUSIVE) {
            emit connectionTestCompleted(-1, QString());
        }
        else {
            char blockedPorts[512];
            LiStringifyPortFlags(portTestResult, "\n", blockedPorts, sizeof(blockedPorts));
            emit connectionTestCompleted(portTestResult, QString(blockedPorts));
        }
    }

signals:
    void connectionTestCompleted(int result, QString blockedPorts);
};

void ComputerModel::testConnectionForComputer(int)
{
    DeferredTestConnectionTask* testConnectionTask = new DeferredTestConnectionTask();
    QObject::connect(testConnectionTask, &DeferredTestConnectionTask::connectionTestCompleted,
                     this, &ComputerModel::connectionTestCompleted);
    QThreadPool::globalInstance()->start(testConnectionTask);
}

void ComputerModel::pairComputer(int computerIndex, QString pin)
{
    Q_ASSERT(computerIndex < m_Computers.count());

    m_ComputerManager->pairHost(m_Computers[computerIndex], pin);
}

void ComputerModel::handlePairingCompleted(NvComputer* computer, QString error)
{
    if (computer != nullptr && error.isEmpty()) {
        QReadLocker lock(&computer->lock);
        if (computer->state == NvComputer::CS_ONLINE
                && computer->pairState == NvComputer::PS_PAIRED) {
            HostAdapterManager::get()->refresh(
                computer->uuid,
                computer->activeAddress,
                computer->activeHttpsPort,
                computer->serverCert);
        }
    }
    emit pairingCompleted(error.isEmpty() ? QVariant() : error);
}

void ComputerModel::handleComputerStateChanged(NvComputer* computer)
{
    QString uuid;
    bool online;
    bool paired;
    NvAddress activeAddress;
    uint16_t httpsPort;
    QSslCertificate serverCert;
    {
        QReadLocker lock(&computer->lock);
        uuid = computer->uuid;
        online = computer->state == NvComputer::CS_ONLINE;
        paired = computer->pairState == NvComputer::PS_PAIRED;
        activeAddress = computer->activeAddress;
        httpsPort = computer->activeHttpsPort;
        serverCert = computer->serverCert;
    }
    if (online && paired) {
        HostAdapterManager::get()->refresh(
            uuid, activeAddress, httpsPort, serverCert);
    }
    auto wake = m_WakeOperations.constFind(uuid);
    if (online && wake != m_WakeOperations.constEnd()
            && (wake->state == QStringLiteral("sending")
                || wake->state == QStringLiteral("sent"))) {
        finishWake(uuid, QStringLiteral("ready"));
    }
    QVector<NvComputer*> newComputerList = m_ComputerManager->getComputers();

    // Reset the model if the structural layout of the list has changed
    if (m_Computers != newComputerList) {
        beginResetModel();
        m_Computers = newComputerList;
        endResetModel();
    }
    else {
        // Let the view know that this specific computer changed
        int index = m_Computers.indexOf(computer);
        emit dataChanged(createIndex(index, 0), createIndex(index, 0));
    }
}

#include "computermodel.moc"
