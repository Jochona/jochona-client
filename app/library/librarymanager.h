#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QVariantList>

class ComputerManager;
class NvComputer;

// Owns cross-Host Library Entry identity and its SQLite projection. Host
// polling remains in ComputerManager; this module turns host-local apps into
// stable, searchable, offline-visible Library Entries.
class LibraryManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList entries READ entries NOTIFY entriesChanged)
    Q_PROPERTY(QString search READ search WRITE setSearch NOTIFY searchChanged)

public:
    static LibraryManager* get();
    static void setComputerManager(ComputerManager* manager);

    QVariantList entries() const;
    QString search() const { return m_Search; }

    Q_INVOKABLE void setSearch(const QString& search);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setFavorite(const QString& entryId, bool favorite);
    Q_INVOKABLE void setHidden(const QString& entryId, bool hidden);
    Q_INVOKABLE bool mergeEntries(const QString& targetEntryId,
                                  const QStringList& sourceEntryIds);
    Q_INVOKABLE QString splitHostApplication(qint64 hostAppId);
    Q_INVOKABLE QString libraryEntryFor(const QString& hostUuid,
                                        int appId) const;
    Q_INVOKABLE QVariantList hostCandidates(const QString& entryId) const;
    Q_INVOKABLE QVariantMap bestHostCandidate(const QString& entryId) const;
    Q_INVOKABLE bool pinHost(const QString& entryId, const QString& hostUuid);
    Q_INVOKABLE bool clearHostPin(const QString& entryId);
    Q_INVOKABLE void recordLaunch(const QString& hostUuid, int appId);

signals:
    void entriesChanged();
    void searchChanged();

private:
    explicit LibraryManager(QObject* parent = nullptr);
    ~LibraryManager() override;

    bool ensureConnection();
    void synchronizeHosts();
    NvComputer* hostForUuid(const QString& uuid) const;

    static LibraryManager* s_Instance;
    static ComputerManager* s_ComputerManager;

    QString m_Search;
    QString m_ConnectionName;
    QSqlDatabase m_Db;
};
