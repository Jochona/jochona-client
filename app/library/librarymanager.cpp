#include "librarymanager.h"

#include "backend/computermanager.h"
#include "backend/nvcomputer.h"
#include "core/settingsdatabase.h"
#include "settings/effectivesettingsresolver.h"

#include <QDateTime>
#include <QReadLocker>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <limits>

LibraryManager* LibraryManager::s_Instance = nullptr;
ComputerManager* LibraryManager::s_ComputerManager = nullptr;

LibraryManager* LibraryManager::get()
{
    if (s_Instance == nullptr) {
        s_Instance = new LibraryManager();
        if (s_ComputerManager != nullptr) {
            setComputerManager(s_ComputerManager);
        }
    }
    return s_Instance;
}

void LibraryManager::setComputerManager(ComputerManager* manager)
{
    if (s_ComputerManager == manager && s_Instance != nullptr) {
        return;
    }
    s_ComputerManager = manager;
    if (s_Instance == nullptr || manager == nullptr) {
        return;
    }
    QObject::connect(manager, &ComputerManager::computerStateChanged,
                     s_Instance, &LibraryManager::refresh,
                     Qt::UniqueConnection);
    QObject::connect(manager, &ComputerManager::computerAddCompleted,
                     s_Instance, &LibraryManager::refresh,
                     Qt::UniqueConnection);
    s_Instance->refresh();
}

LibraryManager::LibraryManager(QObject* parent)
    : QObject(parent)
{
    ensureConnection();
}

LibraryManager::~LibraryManager()
{
    if (m_Db.isValid()) {
        m_Db.close();
        const QString name = m_ConnectionName;
        m_Db = QSqlDatabase();
        QSqlDatabase::removeDatabase(name);
    }
}

bool LibraryManager::ensureConnection()
{
    if (m_Db.isOpen()) {
        return true;
    }
    SettingsDatabase* settings = SettingsDatabase::get();
    if (settings == nullptr || !settings->isOpen()) {
        return false;
    }
    m_ConnectionName = QStringLiteral("LibraryManager-%1")
            .arg(reinterpret_cast<quintptr>(this));
    m_Db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_ConnectionName);
    m_Db.setDatabaseName(settings->databasePath());
    if (!m_Db.open()) {
        qWarning() << "LibraryManager: failed to open database" << m_Db.lastError();
        return false;
    }
    QSqlQuery pragma(m_Db);
    pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON"));
    return true;
}

void LibraryManager::synchronizeHosts()
{
    if (!ensureConnection() || s_ComputerManager == nullptr) {
        return;
    }

    struct AppCopy { int id; QString name; };
    struct HostCopy {
        QString uuid;
        QString name;
        QString address;
        int port;
        QString mac;
        QString manualMac;
        int manualPort;
        QVector<AppCopy> apps;
    };
    QVector<HostCopy> hosts;
    QSet<QString> liveHostIds;

    for (NvComputer* computer : s_ComputerManager->getComputers()) {
        QReadLocker lock(&computer->lock);
        HostCopy host{
            computer->uuid,
            computer->name,
            computer->activeAddress.toString(),
            computer->activeHttpsPort,
            QString::fromLatin1(computer->macAddress.toHex(':')),
            QString::fromLatin1(computer->manualMacAddress.toHex(':')),
            computer->wakePort,
            {},
        };
        for (const NvApp& app : computer->appList) {
            host.apps.append({app.id, app.name});
        }
        liveHostIds.insert(host.uuid);
        hosts.append(host);
    }

    if (!m_Db.transaction()) {
        return;
    }
    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

    for (const HostCopy& host : hosts) {
        QSqlQuery upsertHost(m_Db);
        upsertHost.prepare(QStringLiteral(
            "INSERT INTO hosts (id,name,last_address,last_port,mac,manual_mac,manual_port,updated_at) "
            "VALUES (?,?,?,?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET "
            "name=excluded.name,last_address=excluded.last_address,last_port=excluded.last_port,"
            "mac=excluded.mac,manual_mac=excluded.manual_mac,manual_port=excluded.manual_port,"
            "updated_at=excluded.updated_at"));
        upsertHost.addBindValue(host.uuid);
        upsertHost.addBindValue(host.name);
        upsertHost.addBindValue(host.address);
        upsertHost.addBindValue(host.port);
        upsertHost.addBindValue(host.mac);
        upsertHost.addBindValue(host.manualMac);
        upsertHost.addBindValue(host.manualPort);
        upsertHost.addBindValue(now);
        if (!upsertHost.exec()) {
            qWarning() << "LibraryManager: host sync failed" << upsertHost.lastError();
            m_Db.rollback();
            return;
        }

        QSet<QString> liveApps;
        for (const AppCopy& app : host.apps) {
            const QString appId = QString::number(app.id);
            liveApps.insert(appId);
            QSqlQuery upsertApp(m_Db);
            upsertApp.prepare(QStringLiteral(
                "INSERT INTO host_apps (host_id,app_id,name,kind) VALUES (?,?,?,'game') "
                "ON CONFLICT(host_id,app_id) DO UPDATE SET name=excluded.name"));
            upsertApp.addBindValue(host.uuid);
            upsertApp.addBindValue(appId);
            upsertApp.addBindValue(app.name);
            if (!upsertApp.exec()) {
                qWarning() << "LibraryManager: app sync failed" << upsertApp.lastError();
                m_Db.rollback();
                return;
            }

            QSqlQuery appRow(m_Db);
            appRow.prepare(QStringLiteral(
                "SELECT id FROM host_apps WHERE host_id=? AND app_id=?"));
            appRow.addBindValue(host.uuid);
            appRow.addBindValue(appId);
            if (!appRow.exec() || !appRow.next()) {
                m_Db.rollback();
                return;
            }
            const qint64 hostAppId = appRow.value(0).toLongLong();

            QSqlQuery linked(m_Db);
            linked.prepare(QStringLiteral(
                "SELECT library_entry_id FROM library_entry_apps WHERE host_app_id=?"));
            linked.addBindValue(hostAppId);
            if (!linked.exec() || !linked.next()) {
                const QString entryId = QUuid::createUuid().toString(QUuid::WithoutBraces);
                QSqlQuery createEntry(m_Db);
                createEntry.prepare(QStringLiteral(
                    "INSERT INTO library_entries "
                    "(id,title,kind,created_at,updated_at) VALUES (?,?,'game',?,?)"));
                createEntry.addBindValue(entryId);
                createEntry.addBindValue(app.name);
                createEntry.addBindValue(now);
                createEntry.addBindValue(now);
                if (!createEntry.exec()) {
                    m_Db.rollback();
                    return;
                }
                QSqlQuery link(m_Db);
                link.prepare(QStringLiteral(
                    "INSERT INTO library_entry_apps (library_entry_id,host_app_id) VALUES (?,?)"));
                link.addBindValue(entryId);
                link.addBindValue(hostAppId);
                if (!link.exec()) {
                    m_Db.rollback();
                    return;
                }
            }
        }

        QSqlQuery existing(m_Db);
        existing.prepare(QStringLiteral("SELECT id,app_id FROM host_apps WHERE host_id=?"));
        existing.addBindValue(host.uuid);
        if (existing.exec()) {
            while (existing.next()) {
                if (!liveApps.contains(existing.value(1).toString())) {
                    QSqlQuery remove(m_Db);
                    remove.prepare(QStringLiteral("DELETE FROM host_apps WHERE id=?"));
                    remove.addBindValue(existing.value(0));
                    remove.exec();
                }
            }
        }
    }

    QSqlQuery existingHosts(QStringLiteral("SELECT id FROM hosts"), m_Db);
    while (existingHosts.next()) {
        const QString id = existingHosts.value(0).toString();
        if (!liveHostIds.contains(id)) {
            QSqlQuery remove(m_Db);
            remove.prepare(QStringLiteral("DELETE FROM hosts WHERE id=?"));
            remove.addBindValue(id);
            remove.exec();
        }
    }

    QSqlQuery prune(m_Db);
    prune.exec(QStringLiteral(
        "DELETE FROM library_entries WHERE favorite=0 AND id NOT IN "
        "(SELECT DISTINCT library_entry_id FROM library_entry_apps)"));
    if (!m_Db.commit()) {
        m_Db.rollback();
    }
}

void LibraryManager::refresh()
{
    synchronizeHosts();
    emit entriesChanged();
}

void LibraryManager::setSearch(const QString& search)
{
    const QString normalized = search.trimmed();
    if (m_Search == normalized) {
        return;
    }
    m_Search = normalized;
    emit searchChanged();
    emit entriesChanged();
}

NvComputer* LibraryManager::hostForUuid(const QString& uuid) const
{
    if (s_ComputerManager == nullptr) {
        return nullptr;
    }
    for (NvComputer* computer : s_ComputerManager->getComputers()) {
        QReadLocker lock(&computer->lock);
        if (computer->uuid == uuid) {
            return computer;
        }
    }
    return nullptr;
}

QVariantList LibraryManager::entries() const
{
    QVariantList result;
    if (!const_cast<LibraryManager*>(this)->ensureConnection()) {
        return result;
    }
    QString sql = QStringLiteral(
        "SELECT le.id,le.title,le.kind,le.artwork_path,le.favorite,le.hidden,"
        "COUNT(lea.host_app_id) FROM library_entries le "
        "LEFT JOIN library_entry_apps lea ON lea.library_entry_id=le.id "
        "WHERE le.hidden=0 ");
    if (!m_Search.isEmpty()) {
        sql += QStringLiteral("AND le.title LIKE ? ESCAPE '\\\\' ");
    }
    sql += QStringLiteral(
        "GROUP BY le.id ORDER BY le.favorite DESC, le.title COLLATE NOCASE");
    QSqlQuery query(m_Db);
    query.prepare(sql);
    if (!m_Search.isEmpty()) {
        QString escaped = m_Search;
        escaped.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
        escaped.replace(QStringLiteral("%"), QStringLiteral("\\%"));
        escaped.replace(QStringLiteral("_"), QStringLiteral("\\_"));
        query.addBindValue(QStringLiteral("%") + escaped + QStringLiteral("%"));
    }
    if (!query.exec()) {
        return result;
    }
    while (query.next()) {
        const QString entryId = query.value(0).toString();
        const QVariantList candidates = hostCandidates(entryId);
        int readyCount = 0;
        for (const QVariant& candidate : candidates) {
            if (candidate.toMap().value(QStringLiteral("available")).toBool()) {
                ++readyCount;
            }
        }
        result.append(QVariantMap{
            {QStringLiteral("id"), entryId},
            {QStringLiteral("title"), query.value(1)},
            {QStringLiteral("kind"), query.value(2)},
            {QStringLiteral("artwork"), query.value(3)},
            {QStringLiteral("favorite"), query.value(4).toBool()},
            {QStringLiteral("hidden"), query.value(5).toBool()},
            {QStringLiteral("hostCount"), query.value(6).toInt()},
            {QStringLiteral("readyHostCount"), readyCount},
            {QStringLiteral("available"), readyCount > 0},
        });
    }
    return result;
}

void LibraryManager::setFavorite(const QString& entryId, bool favorite)
{
    if (!ensureConnection()) return;
    QSqlQuery query(m_Db);
    query.prepare(QStringLiteral("UPDATE library_entries SET favorite=?,updated_at=? WHERE id=?"));
    query.addBindValue(favorite);
    query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    query.addBindValue(entryId);
    if (query.exec()) emit entriesChanged();
}

void LibraryManager::setHidden(const QString& entryId, bool hidden)
{
    if (!ensureConnection()) return;
    QSqlQuery query(m_Db);
    query.prepare(QStringLiteral("UPDATE library_entries SET hidden=?,updated_at=? WHERE id=?"));
    query.addBindValue(hidden);
    query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    query.addBindValue(entryId);
    if (query.exec()) emit entriesChanged();
}

QString LibraryManager::libraryEntryFor(const QString& hostUuid, int appId) const
{
    if (!const_cast<LibraryManager*>(this)->ensureConnection()) return {};
    QSqlQuery query(m_Db);
    query.prepare(QStringLiteral(
        "SELECT lea.library_entry_id FROM library_entry_apps lea "
        "JOIN host_apps ha ON ha.id=lea.host_app_id "
        "WHERE ha.host_id=? AND ha.app_id=?"));
    query.addBindValue(hostUuid);
    query.addBindValue(QString::number(appId));
    return query.exec() && query.next() ? query.value(0).toString() : QString();
}

QVariantList LibraryManager::hostCandidates(const QString& entryId) const
{
    QVariantList result;
    if (!const_cast<LibraryManager*>(this)->ensureConnection()) return result;
    QSqlQuery query(m_Db);
    query.prepare(QStringLiteral(
        "SELECT ha.id,ha.host_id,ha.app_id,ha.name,h.name,h.identity_state "
        "FROM library_entry_apps lea "
        "JOIN host_apps ha ON ha.id=lea.host_app_id "
        "JOIN hosts h ON h.id=ha.host_id "
        "WHERE lea.library_entry_id=? ORDER BY h.name COLLATE NOCASE"));
    query.addBindValue(entryId);
    if (!query.exec()) return result;
    while (query.next()) {
        const QString hostUuid = query.value(1).toString();
        NvComputer* host = hostForUuid(hostUuid);
        bool online = false;
        bool paired = false;
        bool wakeable = false;
        bool busy = false;
        int reachability = NvComputer::RI_UNKNOWN;
        QString path = QStringLiteral("Unknown");
        if (host != nullptr) {
            QReadLocker lock(&host->lock);
            online = host->state == NvComputer::CS_ONLINE;
            paired = host->pairState == NvComputer::PS_PAIRED;
            wakeable = !host->macAddress.isEmpty()
                    || !host->manualMacAddress.isEmpty();
            busy = host->currentGameId != 0;
            reachability = host->activeReachability;
            switch (reachability) {
            case NvComputer::RI_LAN:
                path = tr("Direct (LAN)");
                break;
            case NvComputer::RI_TAILNET:
                path = tr("Tailscale");
                break;
            case NvComputer::RI_VPN:
                path = tr("Private network");
                break;
            default:
                break;
            }
        }
        const bool trusted = query.value(5).toString()
                != QStringLiteral("identity_changed");
        result.append(QVariantMap{
            {QStringLiteral("hostAppId"), query.value(0)},
            {QStringLiteral("hostUuid"), hostUuid},
            {QStringLiteral("appId"), query.value(2).toInt()},
            {QStringLiteral("appName"), query.value(3)},
            {QStringLiteral("hostName"), query.value(4)},
            {QStringLiteral("trusted"), trusted},
            {QStringLiteral("online"), online},
            {QStringLiteral("paired"), paired},
            {QStringLiteral("wakeable"), wakeable},
            {QStringLiteral("busy"), busy},
            {QStringLiteral("reachability"), reachability},
            {QStringLiteral("connectionPath"), path},
            {QStringLiteral("available"), trusted && online && paired && !busy},
        });
    }
    return result;
}

QVariantMap LibraryManager::bestHostCandidate(const QString& entryId) const
{
    const QVariantList candidates = hostCandidates(entryId);
    if (candidates.isEmpty() || !const_cast<LibraryManager*>(this)->ensureConnection()) {
        return {};
    }

    QString pinnedHost;
    QSqlQuery pinQuery(m_Db);
    pinQuery.prepare(QStringLiteral(
        "SELECT host_id FROM host_choice_pins WHERE library_entry_id=?"));
    pinQuery.addBindValue(entryId);
    if (pinQuery.exec() && pinQuery.next()) {
        pinnedHost = pinQuery.value(0).toString();
    }

    QVariantMap best;
    int bestScore = std::numeric_limits<int>::min();
    for (const QVariant& value : candidates) {
        QVariantMap candidate = value.toMap();
        QStringList reasons;
        int score = 0;

        const QVariantMap context{
            {QStringLiteral("hostUuid"), candidate.value(QStringLiteral("hostUuid"))},
            {QStringLiteral("appId"), candidate.value(QStringLiteral("appId"))},
            {QStringLiteral("libraryEntryId"), entryId},
        };
        const QVariantMap effective =
                EffectiveSettingsResolver::get()->resolve(context);
        const bool floorSatisfied =
                effective.value(QStringLiteral("floorConflicts")).toMap().isEmpty();
        const bool trusted = candidate.value(QStringLiteral("trusted")).toBool();
        const bool paired = candidate.value(QStringLiteral("paired")).toBool();
        const bool online = candidate.value(QStringLiteral("online")).toBool();
        const bool busy = candidate.value(QStringLiteral("busy")).toBool();
        const bool wakeable = candidate.value(QStringLiteral("wakeable")).toBool();

        if (candidate.value(QStringLiteral("hostUuid")).toString() == pinnedHost) {
            score += 100000;
            reasons << tr("Host Choice Pin");
        }
        if (!trusted) {
            score -= 100000;
            reasons << tr("Host identity changed");
        }
        if (online && paired && !busy) {
            score += 1000;
            reasons << tr("Ready now");
        }
        if (floorSatisfied) {
            score += 250;
            reasons << tr("Meets the Quality Floor");
        }
        if (busy) {
            score -= 1000;
            reasons << tr("Busy");
        }
        if (!online && wakeable) {
            score += 100;
            reasons << tr("Wakeable");
        }
        switch (candidate.value(QStringLiteral("reachability")).toInt()) {
        case NvComputer::RI_LAN:
            score += 300;
            reasons << tr("Direct LAN path");
            break;
        case NvComputer::RI_TAILNET:
            score += 200;
            reasons << tr("Tailscale path");
            break;
        case NvComputer::RI_VPN:
            score += 100;
            reasons << tr("Private-network path");
            break;
        default:
            break;
        }

        QSqlQuery history(m_Db);
        history.prepare(QStringLiteral(
            "SELECT COUNT(*) FROM local_history WHERE kind='session_success' "
            "AND host_id=? AND library_entry_id=?"));
        history.addBindValue(candidate.value(QStringLiteral("hostUuid")));
        history.addBindValue(entryId);
        if (history.exec() && history.next()) {
            const int successes = history.value(0).toInt();
            score += qMin(200, successes * 10);
            if (successes > 0) reasons << tr("Previously successful");
        }

        candidate.insert(QStringLiteral("score"), score);
        candidate.insert(QStringLiteral("floorSatisfied"), floorSatisfied);
        candidate.insert(QStringLiteral("eligible"),
                         trusted && paired && floorSatisfied
                         && (online || wakeable));
        candidate.insert(QStringLiteral("selectionReasons"), reasons);
        if (score > bestScore) {
            bestScore = score;
            best = candidate;
        }
    }
    return best;
}

bool LibraryManager::pinHost(const QString& entryId, const QString& hostUuid)
{
    if (!ensureConnection() || entryId.isEmpty() || hostUuid.isEmpty()) return false;
    QSqlQuery query(m_Db);
    query.prepare(QStringLiteral(
        "INSERT INTO host_choice_pins (library_entry_id,host_id) VALUES (?,?) "
        "ON CONFLICT(library_entry_id) DO UPDATE SET host_id=excluded.host_id"));
    query.addBindValue(entryId);
    query.addBindValue(hostUuid);
    const bool ok = query.exec();
    if (ok) emit entriesChanged();
    return ok;
}

bool LibraryManager::clearHostPin(const QString& entryId)
{
    if (!ensureConnection()) return false;
    QSqlQuery query(m_Db);
    query.prepare(QStringLiteral(
        "DELETE FROM host_choice_pins WHERE library_entry_id=?"));
    query.addBindValue(entryId);
    const bool ok = query.exec();
    if (ok) emit entriesChanged();
    return ok;
}

void LibraryManager::recordLaunch(const QString& hostUuid, int appId)
{
    if (!ensureConnection()) return;
    QSqlQuery lookup(m_Db);
    lookup.prepare(QStringLiteral(
        "SELECT ha.id,lea.library_entry_id FROM host_apps ha "
        "LEFT JOIN library_entry_apps lea ON lea.host_app_id=ha.id "
        "WHERE ha.host_id=? AND ha.app_id=?"));
    lookup.addBindValue(hostUuid);
    lookup.addBindValue(QString::number(appId));
    if (!lookup.exec() || !lookup.next()) return;

    const qint64 hostAppId = lookup.value(0).toLongLong();
    const QString entryId = lookup.value(1).toString();
    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    if (!m_Db.transaction()) return;
    QSqlQuery update(m_Db);
    update.prepare(QStringLiteral(
        "UPDATE host_apps SET last_played=?,play_count=play_count+1 WHERE id=?"));
    update.addBindValue(now);
    update.addBindValue(hostAppId);
    if (!update.exec()) { m_Db.rollback(); return; }
    QSqlQuery history(m_Db);
    history.prepare(QStringLiteral(
        "INSERT INTO local_history "
        "(ts,kind,host_id,library_entry_id,host_app_id) VALUES (?,'session_launch',?,?,?)"));
    history.addBindValue(now);
    history.addBindValue(hostUuid);
    history.addBindValue(entryId);
    history.addBindValue(hostAppId);
    if (!history.exec() || !m_Db.commit()) { m_Db.rollback(); return; }
    emit entriesChanged();
}

bool LibraryManager::mergeEntries(const QString& targetEntryId,
                                  const QStringList& sourceEntryIds)
{
    if (!ensureConnection() || targetEntryId.isEmpty() || sourceEntryIds.isEmpty()) return false;
    if (!m_Db.transaction()) return false;
    for (const QString& source : sourceEntryIds) {
        if (source.isEmpty() || source == targetEntryId) continue;
        QSqlQuery move(m_Db);
        move.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO library_entry_apps (library_entry_id,host_app_id,metadata_json) "
            "SELECT ?,host_app_id,metadata_json FROM library_entry_apps WHERE library_entry_id=?"));
        move.addBindValue(targetEntryId);
        move.addBindValue(source);
        if (!move.exec()) { m_Db.rollback(); return false; }
        QSqlQuery remove(m_Db);
        remove.prepare(QStringLiteral("DELETE FROM library_entries WHERE id=?"));
        remove.addBindValue(source);
        if (!remove.exec()) { m_Db.rollback(); return false; }
    }
    if (!m_Db.commit()) { m_Db.rollback(); return false; }
    emit entriesChanged();
    return true;
}

QString LibraryManager::splitHostApplication(qint64 hostAppId)
{
    if (!ensureConnection() || hostAppId <= 0) return {};
    QSqlQuery query(m_Db);
    query.prepare(QStringLiteral("SELECT name FROM host_apps WHERE id=?"));
    query.addBindValue(hostAppId);
    if (!query.exec() || !query.next()) return {};
    const QString entryId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    if (!m_Db.transaction()) return {};
    QSqlQuery create(m_Db);
    create.prepare(QStringLiteral(
        "INSERT INTO library_entries (id,title,kind,created_at,updated_at) VALUES (?,?,'game',?,?)"));
    create.addBindValue(entryId);
    create.addBindValue(query.value(0));
    create.addBindValue(now);
    create.addBindValue(now);
    if (!create.exec()) { m_Db.rollback(); return {}; }
    QSqlQuery move(m_Db);
    move.prepare(QStringLiteral("UPDATE library_entry_apps SET library_entry_id=? WHERE host_app_id=?"));
    move.addBindValue(entryId);
    move.addBindValue(hostAppId);
    if (!move.exec() || !m_Db.commit()) { m_Db.rollback(); return {}; }
    emit entriesChanged();
    return entryId;
}
