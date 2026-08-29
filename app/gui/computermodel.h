#include "backend/computermanager.h"
#include "streaming/session.h"

#include <QAbstractListModel>
#include <QHash>
#include <QPointer>
#include <QTimer>

class ComputerModel : public QAbstractListModel
{
    Q_OBJECT

    enum Roles
    {
        NameRole = Qt::UserRole,
        OnlineRole,
        PairedRole,
        BusyRole,
        WakeableRole,
        StatusUnknownRole,
        ServerSupportedRole,
        DetailsRole,
        WakePortRole,
        WakeBroadcastRole,
        ManualMacRole,
        ConnectionPathRole,
        UuidRole,
        IndexRole,
        WakeStateRole,
        WakeProviderRole,
        WakeErrorRole
    };

public:
    explicit ComputerModel(QObject* object = nullptr);

    // Must be called before any QAbstractListModel functions
    Q_INVOKABLE void initialize(ComputerManager* computerManager);

    QVariant data(const QModelIndex &index, int role) const override;

    int rowCount(const QModelIndex &parent) const override;

    virtual QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void deleteComputer(int computerIndex);

    Q_INVOKABLE QString generatePinString();

    Q_INVOKABLE void pairComputer(int computerIndex, QString pin);

    Q_INVOKABLE void testConnectionForComputer(int computerIndex);

    Q_INVOKABLE bool wakeComputer(int computerIndex,
                                  bool continueWhenReady = false,
                                  int appId = -1,
                                  bool forceDirect = false);

    Q_INVOKABLE void renameComputer(int computerIndex, QString name);

    // Wake-on-LAN overrides for the details screen (proposal §6.5). mac is
    // colon-hex ("" clears), port 0 = automatic, broadcast "" = all NICs.
    Q_INVOKABLE void setWakeOverrides(int computerIndex, QString mac, int port, QString broadcast);

    // Skip the polling sleep for this host and re-check it right now.
    Q_INVOKABLE void reprobeComputer(int computerIndex);

    Q_INVOKABLE Session* createSessionForCurrentGame(int computerIndex);

    // Jochona: recent-route lookups resolve stored host uuids against the
    // live model. -1 when no host matches.
    Q_INVOKABLE int indexOfUuid(const QString& uuid) const;

    Q_INVOKABLE bool isOnlinePaired(int computerIndex) const;

    Q_INVOKABLE QString nameForIndex(int computerIndex) const;

    Q_INVOKABLE QString uuidForIndex(int computerIndex) const;

    // Jochona: everything the host route header needs in one shot
    // (name, state, wakeability, summary, and connection path).
    Q_INVOKABLE QVariantMap hostInfoForIndex(int computerIndex) const;

signals:
    void pairingCompleted(QVariant error);
    void connectionTestCompleted(int result, QString blockedPorts);
    void wakeStateChanged(int computerIndex, QString state);
    void wakeReady(int computerIndex, int appId);

private slots:
    void handleComputerStateChanged(NvComputer* computer);

    void handlePairingCompleted(NvComputer* computer, QString error);
    void handleWakeAttempt(const QString& uuid,
                           int generation,
                           bool sent,
                           QString error);

private:
    struct WakeOperation
    {
        int generation = 0;
        int attemptsFinished = 0;
        int attemptsExpected = 0;
        bool anySent = false;
        bool continueWhenReady = false;
        int appId = -1;
        QString state;
        QString error;
        QString providerName;
        QPointer<QTimer> probeTimer;
    };

    void setWakeState(const QString& uuid, const QString& state);
    void finishWake(const QString& uuid, const QString& state);

    QVector<NvComputer*> m_Computers;
    ComputerManager* m_ComputerManager;
    QHash<QString, WakeOperation> m_WakeOperations;
};
