#include "autoupdatechecker.h"

#include "core/settingsdatabase.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>

namespace {
const QUrl kReleasesUrl(
    QStringLiteral("https://api.github.com/repos/Jochona/jochona-client/releases?per_page=20"));
}

AutoUpdateChecker::AutoUpdateChecker(QObject* parent)
    : QObject(parent)
    , m_CurrentVersion(parseVersion(QStringLiteral(VERSION_STR)))
    , m_Channel(QStringLiteral("stable"))
    , m_CheckEnabled(true)
{
    SettingsDatabase* database = SettingsDatabase::get();
    if (database != nullptr && database->isOpen()) {
        m_Channel = database->setting(
            QStringLiteral("updates.channel"),
            QStringLiteral("stable")).toString();
        if (m_Channel != QStringLiteral("preview")) {
            m_Channel = QStringLiteral("stable");
        }
        m_CheckEnabled = database->setting(
            QStringLiteral("updates.check_enabled"), true).toBool();
    }
    m_Nam.setStrictTransportSecurityEnabled(true);
    m_Nam.setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
    connect(&m_Nam, &QNetworkAccessManager::finished,
            this, &AutoUpdateChecker::handleUpdateCheckRequestFinished);
}

void AutoUpdateChecker::setChannel(const QString& channel)
{
    const QString normalized =
        channel == QStringLiteral("preview")
            ? QStringLiteral("preview") : QStringLiteral("stable");
    if (m_Channel == normalized) return;
    m_Channel = normalized;
    if (SettingsDatabase* database = SettingsDatabase::get()) {
        database->setSetting(QStringLiteral("updates.channel"), m_Channel);
    }
    emit channelChanged();
    m_AvailableVersion.clear();
    m_AvailableUrl.clear();
    emit updateStateChanged();
}

void AutoUpdateChecker::setCheckEnabled(bool enabled)
{
    if (m_CheckEnabled == enabled) return;
    m_CheckEnabled = enabled;
    if (SettingsDatabase* database = SettingsDatabase::get()) {
        database->setSetting(QStringLiteral("updates.check_enabled"),
                             enabled);
    }
    emit checkEnabledChanged();
}

void AutoUpdateChecker::start()
{
    if (m_Started || !m_CheckEnabled) return;
    m_Started = true;
    checkNow();
}

void AutoUpdateChecker::checkNow()
{
    if (m_Status == QStringLiteral("checking")) return;
    setStatus(QStringLiteral("checking"));
    QNetworkRequest request(kReleasesUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Jochona/%1").arg(
                          QStringLiteral(VERSION_STR)));
    request.setRawHeader("Accept",
                         "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);
#endif
    m_Nam.get(request);
}

QVector<int> AutoUpdateChecker::parseVersion(const QString& string)
{
    QVector<int> version;
    QRegularExpressionMatchIterator matches =
        QRegularExpression(QStringLiteral("(\\d+)"))
            .globalMatch(string);
    while (matches.hasNext()) {
        version.append(matches.next().captured(1).toInt());
    }
    return version;
}

int AutoUpdateChecker::compareVersion(const QVector<int>& first,
                                      const QVector<int>& second)
{
    const int length = qMax(first.size(), second.size());
    for (int index = 0; index < length; ++index) {
        const int a = index < first.size() ? first.at(index) : 0;
        const int b = index < second.size() ? second.at(index) : 0;
        if (a < b) return -1;
        if (a > b) return 1;
    }
    return 0;
}

void AutoUpdateChecker::handleUpdateCheckRequestFinished(
        QNetworkReply* reply)
{
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        setStatus(QStringLiteral("failed"));
        return;
    }
    QJsonParseError parseError {};
    const QJsonDocument document =
        QJsonDocument::fromJson(reply->readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError
            || !document.isArray()) {
        setStatus(QStringLiteral("failed"));
        return;
    }

    QVector<int> newestVersion;
    QString newestLabel;
    QUrl newestUrl;
    for (const QJsonValue& value : document.array()) {
        if (!value.isObject()) continue;
        const QJsonObject release = value.toObject();
        if (release.value(QStringLiteral("draft")).toBool()) continue;
        if (m_Channel == QStringLiteral("stable")
                && release.value(QStringLiteral("prerelease")).toBool()) {
            continue;
        }
        const QString label =
            release.value(QStringLiteral("tag_name")).toString();
        const QVector<int> version = parseVersion(label);
        if (version.isEmpty()) continue;
        if (newestVersion.isEmpty()
                || compareVersion(newestVersion, version) < 0) {
            newestVersion = version;
            newestLabel = label;
            newestUrl = QUrl(
                release.value(QStringLiteral("html_url")).toString());
        }
    }
    if (newestVersion.isEmpty()) {
        setStatus(QStringLiteral("failed"));
        return;
    }
    if (compareVersion(m_CurrentVersion, newestVersion) < 0) {
        m_AvailableVersion = newestLabel;
        m_AvailableUrl = newestUrl;
        setStatus(QStringLiteral("available"));
        emit updateStateChanged();
        emit updateAvailable(m_AvailableVersion, m_AvailableUrl);
    } else {
        m_AvailableVersion.clear();
        m_AvailableUrl.clear();
        emit updateStateChanged();
        setStatus(QStringLiteral("upToDate"));
    }
}

void AutoUpdateChecker::setStatus(const QString& status)
{
    if (m_Status == status) return;
    m_Status = status;
    emit statusChanged();
}
