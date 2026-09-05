#include "supportbundlemanager.h"

#include "core/settingsdatabase.h"
#include "path.h"
#include "settings/effectivesettingsresolver.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QSysInfo>
#include <QUuid>

#include <QStandardPaths>
SupportBundleManager* SupportBundleManager::s_Instance = nullptr;

SupportBundleManager* SupportBundleManager::get()
{
    if (s_Instance == nullptr) {
        s_Instance = new SupportBundleManager();
    }
    return s_Instance;
}

SupportBundleManager::SupportBundleManager(QObject* parent)
    : QObject(parent)
    , m_ConnectionName(QStringLiteral("SupportBundle-%1").arg(
          QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
    if (SettingsDatabase* database = SettingsDatabase::get()) {
        m_HistoryRetentionDays = database->historyRetentionDays();
        database->setHistoryRetentionDays(m_HistoryRetentionDays);
    }
    refreshPreview();
}

SupportBundleManager::~SupportBundleManager()
{
    if (m_Db.isValid()) {
        m_Db.close();
        m_Db = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_ConnectionName);
    }
}

bool SupportBundleManager::ensureConnection()
{
    if (m_Db.isOpen()) return true;
    SettingsDatabase* settings = SettingsDatabase::get();
    if (settings == nullptr || !settings->isOpen()) return false;
    m_Db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                     m_ConnectionName);
    m_Db.setDatabaseName(settings->databasePath());
    m_Db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
    return m_Db.open();
}

void SupportBundleManager::setIncludeAddresses(bool include)
{
    if (m_IncludeAddresses == include) return;
    m_IncludeAddresses = include;
    emit includeAddressesChanged();
    refreshPreview();
}

void SupportBundleManager::setHistoryRetentionDays(int days)
{
    days = qBound(0, days, 3650);
    if (m_HistoryRetentionDays == days) return;
    SettingsDatabase* database = SettingsDatabase::get();
    if (database == nullptr || !database->setHistoryRetentionDays(days)) {
        setExportStatus(tr("Could not update Local History retention."));
        return;
    }
    m_HistoryRetentionDays = days;
    emit historyRetentionDaysChanged();
    refreshPreview();
}

bool SupportBundleManager::clearHistory()
{
    SettingsDatabase* database = SettingsDatabase::get();
    if (database == nullptr || !database->clearLocalHistory()) {
        setExportStatus(tr("Could not clear Local History."));
        return false;
    }
    setExportStatus(tr("Local History cleared."));
    refreshPreview();
    return true;
}

QString SupportBundleManager::pseudonym(const QString& value,
                                        const QString& prefix)
{
    if (value.isEmpty()) return {};
    const QByteArray digest = QCryptographicHash::hash(
        value.toUtf8(), QCryptographicHash::Sha256).toHex().left(10);
    return prefix + QLatin1Char('-') + QString::fromLatin1(digest);
}

QString SupportBundleManager::redactLog(QString text, bool includeAddresses)
{
    text.replace(QDir::homePath(), QStringLiteral("<home>"));
    text.replace(QRegularExpression(
        QStringLiteral("(?i)(unique id[^\\\"]*\\\")([^\\\"]+)(\\\")")),
        QStringLiteral("\\1<id-redacted>\\3"));
    text.replace(QRegularExpression(
        QStringLiteral("-----BEGIN [^-]+-----[\\s\\S]*?-----END [^-]+-----")),
        QStringLiteral("<private-material-redacted>"));
    text.replace(QRegularExpression(
        QStringLiteral("(?im)^.*clipboard.*$")),
        QStringLiteral("<clipboard-event-redacted>"));
    text.replace(QRegularExpression(
        QStringLiteral("(?i)(token|password|passphrase|secret|pin)"
                       "\\s*[:=]\\s*[^\\s,;]+")),
        QStringLiteral("\\1=<redacted>"));
    // Defense-in-depth for log excerpts written before nvhttp.cpp stopped
    // logging these as raw request-URL query parameters (pairing salt,
    // client certificate, challenge/response, pairing secret) or the
    // per-request unique ID/uuid that appears on every call.
    text.replace(QRegularExpression(
        QStringLiteral("(?i)\\b(uniqueid|uuid|salt|clientcert|"
                       "clientchallenge|serverchallengeresp|"
                       "clientpairingsecret)=[0-9A-Za-z%]+")),
        QStringLiteral("\\1=<redacted>"));
    text.replace(QRegularExpression(
        QStringLiteral("\\b[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-"
                       "[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-"
                       "[0-9a-fA-F]{12}\\b")),
        QStringLiteral("<id-redacted>"));
    text.replace(QRegularExpression(
        QStringLiteral("\\b(?:[0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}\\b")),
        QStringLiteral("<mac-redacted>"));
    if (!includeAddresses) {
        text.replace(QRegularExpression(
            QStringLiteral("\\b(?:\\d{1,3}\\.){3}\\d{1,3}\\b")),
            QStringLiteral("<address-redacted>"));
        text.replace(QRegularExpression(
            QStringLiteral("\\b(?:[0-9A-Fa-f]{1,4}:){4,7}"
                           "[0-9A-Fa-f]{0,4}\\b")),
            QStringLiteral("<address-redacted>"));
    }
    return text;
}

QJsonObject SupportBundleManager::buildBundle()
{
    QJsonObject bundle;
    bundle.insert(QStringLiteral("schema"), 1);
    bundle.insert(QStringLiteral("generatedAt"),
                  QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    bundle.insert(QStringLiteral("clientVersion"), QStringLiteral(VERSION_STR));
    bundle.insert(QStringLiteral("os"), QSysInfo::prettyProductName());
    bundle.insert(QStringLiteral("architecture"),
                  QSysInfo::currentCpuArchitecture());
    bundle.insert(QStringLiteral("telemetryEnabled"), false);
    bundle.insert(QStringLiteral("addressesIncluded"), m_IncludeAddresses);
    bundle.insert(QStringLiteral("historyRetentionDays"),
                  m_HistoryRetentionDays);

    const QVariantMap effective =
        EffectiveSettingsResolver::get()->resolve({});
    QJsonObject settings;
    settings.insert(QStringLiteral("values"),
                    QJsonObject::fromVariantMap(
                        effective.value(QStringLiteral("values")).toMap()));
    settings.insert(QStringLiteral("provenance"),
                    QJsonObject::fromVariantMap(
                        effective.value(QStringLiteral("provenance")).toMap()));
    settings.insert(QStringLiteral("floorConflicts"),
                    QJsonObject::fromVariantMap(
                        effective.value(QStringLiteral("floorConflicts")).toMap()));
    bundle.insert(QStringLiteral("effectiveSettings"), settings);

    if (ensureConnection()) {
        QJsonArray hosts;
        QSqlQuery hostQuery(QStringLiteral(
            "SELECT id,name,last_address,last_port,paired,identity_state "
            "FROM hosts ORDER BY sort_order,name"), m_Db);
        while (hostQuery.next()) {
            QJsonObject host;
            host.insert(QStringLiteral("id"),
                        pseudonym(hostQuery.value(0).toString(),
                                  QStringLiteral("host")));
            host.insert(QStringLiteral("name"),
                        pseudonym(hostQuery.value(1).toString(),
                                  QStringLiteral("rig")));
            host.insert(QStringLiteral("paired"),
                        hostQuery.value(4).toBool());
            host.insert(QStringLiteral("identityState"),
                        hostQuery.value(5).toString());
            if (m_IncludeAddresses) {
                host.insert(QStringLiteral("address"),
                            hostQuery.value(2).toString());
                host.insert(QStringLiteral("port"),
                            hostQuery.value(3).toInt());
            }
            hosts.append(host);
        }
        bundle.insert(QStringLiteral("hosts"), hosts);

        QJsonArray capabilities;
        QSqlQuery capabilityQuery(QStringLiteral(
            "SELECT host_id,capabilities_json,confidence,verified_at "
            "FROM capability_cache ORDER BY host_id"), m_Db);
        while (capabilityQuery.next()) {
            QJsonObject capability;
            capability.insert(QStringLiteral("host"),
                              pseudonym(capabilityQuery.value(0).toString(),
                                        QStringLiteral("host")));
            const QJsonDocument document = QJsonDocument::fromJson(
                capabilityQuery.value(1).toByteArray());
            capability.insert(QStringLiteral("capabilities"),
                              document.isObject() ? document.object()
                                                  : QJsonObject());
            capability.insert(QStringLiteral("confidence"),
                              capabilityQuery.value(2).toString());
            capability.insert(QStringLiteral("verifiedAt"),
                              capabilityQuery.value(3).toString());
            capabilities.append(capability);
        }
        bundle.insert(QStringLiteral("capabilities"), capabilities);

        // Support Bundle contract (CONTEXT.md "Support Bundle"): route
        // state is part of "capabilities, route state, and recent
        // failures" -- surface Wake Provider choice and Beacon identity
        // state pulled straight from SettingsDatabase, the single durable
        // authority for this data.
        QJsonArray wakeRoutes;
        QJsonArray beaconRoutes;
        if (SettingsDatabase* settingsDb = SettingsDatabase::get()) {
            for (const QVariant& value : settingsDb->wakeRoutes()) {
                const QVariantMap route = value.toMap();
                QJsonObject entry;
                entry.insert(QStringLiteral("host"),
                            pseudonym(route.value(QStringLiteral("hostId")).toString(),
                                      QStringLiteral("host")));
                entry.insert(QStringLiteral("provider"),
                            route.value(QStringLiteral("provider")).toString());
                const QString beaconId =
                        route.value(QStringLiteral("beaconId")).toString();
                if (!beaconId.isEmpty()) {
                    entry.insert(QStringLiteral("beacon"),
                                pseudonym(beaconId, QStringLiteral("beacon")));
                    entry.insert(QStringLiteral("beaconHost"),
                                pseudonym(route.value(QStringLiteral("beaconHostId"))
                                                  .toString(),
                                          QStringLiteral("host")));
                }
                entry.insert(QStringLiteral("updatedAt"),
                            route.value(QStringLiteral("updatedAt")).toString());
                wakeRoutes.append(entry);
            }

            for (const QVariant& value : settingsDb->beacons()) {
                const QVariantMap beacon = value.toMap();
                QJsonObject entry;
                entry.insert(QStringLiteral("beacon"),
                            pseudonym(beacon.value(QStringLiteral("id")).toString(),
                                      QStringLiteral("beacon")));
                entry.insert(QStringLiteral("name"),
                            pseudonym(beacon.value(QStringLiteral("name")).toString(),
                                      QStringLiteral("beacon-name")));
                entry.insert(QStringLiteral("identityState"),
                            beacon.value(QStringLiteral("identityState")).toString());
                entry.insert(QStringLiteral("spkiFingerprint"),
                            pseudonym(beacon.value(QStringLiteral("spkiFingerprint"))
                                              .toString(),
                                      QStringLiteral("spki")));
                if (m_IncludeAddresses) {
                    entry.insert(QStringLiteral("url"),
                                beacon.value(QStringLiteral("url")).toString());
                }
                entry.insert(QStringLiteral("updatedAt"),
                            beacon.value(QStringLiteral("updatedAt")).toString());
                beaconRoutes.append(entry);
            }
        }
        bundle.insert(QStringLiteral("wakeRoutes"), wakeRoutes);
        bundle.insert(QStringLiteral("beacons"), beaconRoutes);

        QJsonArray displays;
        QSqlQuery displayQuery(QStringLiteral(
            "SELECT name,dock_state,width,height,refresh_hz,hdr_capable,"
            "last_seen FROM display_contexts ORDER BY last_seen DESC"), m_Db);
        while (displayQuery.next()) {
            displays.append(QJsonObject{
                {QStringLiteral("name"), displayQuery.value(0).toString()},
                {QStringLiteral("dockState"),
                 pseudonym(displayQuery.value(1).toString(),
                           QStringLiteral("topology"))},
                {QStringLiteral("width"), displayQuery.value(2).toInt()},
                {QStringLiteral("height"), displayQuery.value(3).toInt()},
                {QStringLiteral("refreshHz"), displayQuery.value(4).toDouble()},
                {QStringLiteral("hdrCapable"), displayQuery.value(5).toBool()},
                {QStringLiteral("lastSeen"), displayQuery.value(6).toString()},
            });
        }
        bundle.insert(QStringLiteral("displayContexts"), displays);

        QJsonArray history;
        if (m_HistoryRetentionDays > 0) {
            QSqlQuery historyQuery(m_Db);
            historyQuery.prepare(QStringLiteral(
                "SELECT ts,kind,host_id,library_entry_id,summary_json "
                "FROM local_history WHERE ts>=? ORDER BY ts DESC LIMIT 200"));
            historyQuery.addBindValue(
                QDateTime::currentDateTimeUtc()
                    .addDays(-m_HistoryRetentionDays)
                    .toString(Qt::ISODateWithMs));
            if (historyQuery.exec()) {
                while (historyQuery.next()) {
                    history.append(QJsonObject{
                        {QStringLiteral("timestamp"),
                         historyQuery.value(0).toString()},
                        {QStringLiteral("kind"),
                         historyQuery.value(1).toString()},
                        {QStringLiteral("host"),
                         pseudonym(historyQuery.value(2).toString(),
                                   QStringLiteral("host"))},
                        {QStringLiteral("libraryEntry"),
                         pseudonym(historyQuery.value(3).toString(),
                                   QStringLiteral("entry"))},
                        {QStringLiteral("summary"),
                         redactLog(historyQuery.value(4).toString(),
                                   m_IncludeAddresses)},
                    });
                }
            }
        }
        bundle.insert(QStringLiteral("localHistory"), history);
    }

    QJsonArray logs;
    QDir logDirectory(Path::getLogDir());
    const QStringList names = logDirectory.entryList(
        {QStringLiteral("Jochona-*.log"), QStringLiteral("Moonlight-*.log")},
        QDir::Files, QDir::Time);
    QVariantMap hostAliases;
    if (ensureConnection()) {
        QSqlQuery aliases(QStringLiteral("SELECT id,name FROM hosts"), m_Db);
        while (aliases.next()) {
            hostAliases.insert(
                aliases.value(1).toString(),
                pseudonym(aliases.value(0).toString(),
                          QStringLiteral("host")));
        }
    }
    for (int index = 0; index < qMin(3, names.size()); ++index) {
        QFile file(logDirectory.filePath(names.at(index)));
        if (!file.open(QIODevice::ReadOnly)) continue;
        constexpr qint64 kMaxExcerpt = 48 * 1024;
        if (file.size() > kMaxExcerpt) file.seek(file.size() - kMaxExcerpt);
        QString excerpt = redactLog(
            QString::fromUtf8(file.read(kMaxExcerpt)), m_IncludeAddresses);
        for (auto alias = hostAliases.constBegin();
             alias != hostAliases.constEnd(); ++alias) {
            if (!alias.key().isEmpty()) {
                excerpt.replace(alias.key(), alias.value().toString(),
                                Qt::CaseInsensitive);
            }
        }
        logs.append(QJsonObject{
            {QStringLiteral("file"), QStringLiteral("log-%1").arg(index + 1)},
            {QStringLiteral("excerpt"), excerpt},
        });
    }
    bundle.insert(QStringLiteral("logs"), logs);
    return bundle;
}

void SupportBundleManager::refreshPreview()
{
    m_PreviewBundle = buildBundle();
    m_PreviewText = QString::fromUtf8(
        QJsonDocument(m_PreviewBundle).toJson(QJsonDocument::Indented));
    emit previewChanged();
}

bool SupportBundleManager::exportBundle(const QUrl& destination)
{
    QString path = destination.isLocalFile()
        ? destination.toLocalFile() : destination.toString();
    if (path.isEmpty()) {
        setExportStatus(tr("Choose a file for the Support Bundle."));
        return false;
    }
    refreshPreview();
    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly)) {
        setExportStatus(tr("Could not open the Support Bundle destination."));
        return false;
    }
    output.write(QJsonDocument(m_PreviewBundle)
                     .toJson(QJsonDocument::Indented));
    if (!output.commit()) {
        setExportStatus(tr("Could not finish writing the Support Bundle."));
        return false;
    }
    setExportStatus(tr("Support Bundle exported to %1.").arg(path));
    return true;
}

bool SupportBundleManager::exportDefaultBundle()
{
    QString directory = QStandardPaths::writableLocation(
        QStandardPaths::DocumentsLocation);
    if (directory.isEmpty()) directory = QDir::homePath();
    QDir().mkpath(directory);
    const QString path = QDir(directory).filePath(
        QStringLiteral("Jochona-Support-%1.json").arg(
            QDateTime::currentDateTimeUtc().toString(
                QStringLiteral("yyyyMMdd-HHmmss"))));
    const bool exported = exportBundle(QUrl::fromLocalFile(path));
    if (exported && m_IncludeAddresses) {
        setIncludeAddresses(false);
    }
    return exported;
}

void SupportBundleManager::setExportStatus(const QString& status)
{
    if (m_ExportStatus == status) return;
    m_ExportStatus = status;
    emit exportStatusChanged();
}
