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
    Q_PROPERTY(bool showHidden READ showHidden WRITE setShowHidden NOTIFY showHiddenChanged)

public:
    static LibraryManager* get();
    static void setComputerManager(ComputerManager* manager);

    QVariantList entries() const;
    QString search() const { return m_Search; }
    bool showHidden() const { return m_ShowHidden; }

    Q_INVOKABLE void setSearch(const QString& search);
    Q_INVOKABLE void setShowHidden(bool showHidden);
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
    // Writes a redacted, structured Local History record for how a Session
    // ended: kind='session_success' or 'session_failure' with a
    // summary_json of {stage, errorCode} only -- never raw host addresses,
    // certificates, or free-form error text. stage is a short protocol
    // phase name (e.g. from LiGetStageName()) or a fixed literal such as
    // "initialize"/"launch_error"; callers must not pass user- or
    // host-supplied free text here.
    Q_INVOKABLE void recordSessionOutcome(const QString& hostUuid, int appId,
                                          bool success, const QString& stage,
                                          int errorCode);

signals:
    void entriesChanged();
    void searchChanged();
    void showHiddenChanged();

private:
    explicit LibraryManager(QObject* parent = nullptr);
    ~LibraryManager() override;

    bool ensureConnection();
    void synchronizeHosts();
    NvComputer* hostForUuid(const QString& uuid) const;

    static LibraryManager* s_Instance;
    static ComputerManager* s_ComputerManager;

    QString m_Search;
    bool m_ShowHidden = false;
    QString m_ConnectionName;
    QSqlDatabase m_Db;
};
