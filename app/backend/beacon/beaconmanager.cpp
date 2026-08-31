//
// SPDX-FileCopyrightText: Jochona Client Contributors
// SPDX-License-Identifier: GPL-3.0-only
//
#include "beaconmanager.h"

#include "backend/identitymanager.h"
#include "beaconprotocol.h"
#include "core/settingsdatabase.h"
#include "spake2client.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRunnable>
#include <QSslError>
#include <QThreadPool>
#include <QTimer>
#include <QUrl>
#include <QUuid>


namespace
{
constexpr int kRequestTimeoutMs = 8000;
constexpr int kDefaultBeaconPort = 47100;
constexpr char kApiRoot[] = "/jochona/beacon/v1";

struct HttpResult
{
    int status = 0;
    QByteArray body;
    QSslCertificate peerCertificate;
    QByteArray peerSpkiFingerprint;
    bool identityChanged = false;
    QString error;

    bool ok() const { return status >= 200 && status < 300 && error.isEmpty(); }
};

QByteArray spkiFingerprint(const QSslCertificate& certificate)
{
    return BeaconSpake2Client::certificateSpkiSha256(certificate.toDer());
}

QUrl canonicalBaseUrl(QString value)
{
    value = value.trimmed();
    if (!value.contains(QLatin1String("://"))) {
        value.prepend(QStringLiteral("https://"));
    }
    QUrl url(value);
    if (url.scheme().isEmpty()) url.setScheme(QStringLiteral("https"));
    if (url.port() <= 0) url.setPort(kDefaultBeaconPort);
    url.setPath(QString());
    url.setQuery(QString());
    url.setFragment(QString());
    return url;
}

QUrl apiUrl(const QUrl& base, const QString& suffix)
{
    QUrl url(base);
    url.setPath(QString::fromLatin1(kApiRoot) + suffix);
    return url;
}

QSslCertificate clientCertificate()
{
    const QList<QSslCertificate> certificates = QSslCertificate::fromData(
        IdentityManager::get()->getCertificate(), QSsl::Pem);
    return certificates.isEmpty() ? QSslCertificate() : certificates.first();
}

HttpResult request(const QUrl& url,
                   const QByteArray& method,
                   const QByteArray& body,
                   const QByteArray& expectedSpki,
                   bool bootstrap,
                   const QHash<QByteArray, QByteArray>& headers = {})
{
    HttpResult result;
    if (!url.isValid() || url.scheme() != QLatin1String("https")) {
        result.error = QStringLiteral("Beacon URL must be valid HTTPS");
        return result;
    }

    QNetworkAccessManager manager;
    manager.setProxy(QNetworkProxy::NoProxy);
    QNetworkRequest networkRequest(url);
    QSslConfiguration ssl = IdentityManager::get()->getSslConfig();
    ssl.setPeerVerifyMode(QSslSocket::VerifyPeer);
    networkRequest.setSslConfiguration(ssl);
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader,
                             QStringLiteral("application/json"));
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    networkRequest.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
#endif
    for (auto it = headers.constBegin(); it != headers.constEnd(); ++it) {
        networkRequest.setRawHeader(it.key(), it.value());
    }

    QByteArray observedDuringHandshake;
    bool pinMismatch = false;

    // Shared by both handlers below: accepts a peer's SPKI fingerprint
    // under bootstrap TOFU (first-observed-wins, flagging any later
    // disagreement within this same request) or, once paired, exact
    // match against expectedSpki.
    auto acceptFingerprint = [&](const QByteArray& fingerprint) {
        if (fingerprint.isEmpty()) {
            return false;
        }
        if (bootstrap) {
            if (!observedDuringHandshake.isEmpty()
                    && observedDuringHandshake != fingerprint) {
                return false;
            }
            observedDuringHandshake = fingerprint;
            return true;
        }
        return fingerprint == expectedSpki;
    };

    QObject::connect(
        &manager, &QNetworkAccessManager::sslErrors,
        &manager,
        [&](QNetworkReply* reply, const QList<QSslError>& errors) {
            QSslCertificate leaf = reply->sslConfiguration().peerCertificate();
            if (leaf.isNull()) {
                for (const QSslError& sslError : errors) {
                    if (!sslError.certificate().isNull()) {
                        leaf = sslError.certificate();
                        break;
                    }
                }
            }
            if (acceptFingerprint(spkiFingerprint(leaf))) {
                // Beacon presents a self-signed certificate outside
                // bootstrap TOFU, so chain-trust failure here is expected;
                // let the handshake continue now that the pin matches. A
                // peer whose certificate chains to a public CA never
                // raises sslErrors at all -- encrypted() below is what
                // actually stops that case, before any request data is
                // sent, so pinning cannot be bypassed by presenting a
                // CA-trusted impostor certificate.
                reply->ignoreSslErrors(errors);
            } else {
                pinMismatch = true;
            }
        });

    QObject::connect(
        &manager, &QNetworkAccessManager::encrypted,
        &manager,
        [&](QNetworkReply* reply) {
            if (pinMismatch) {
                // Already rejected in sslErrors above, which did not call
                // ignoreSslErrors -- Qt will fail the handshake on its own.
                return;
            }
            const QByteArray fingerprint =
                spkiFingerprint(reply->sslConfiguration().peerCertificate());
            if (!acceptFingerprint(fingerprint)) {
                // The handshake just completed -- no user data has been
                // transmitted yet -- but the peer's SPKI does not match
                // what we pinned. Abort now, before any HTTP request data
                // (headers or body, including SPAKE2 shares/pairing codes)
                // leaves the socket.
                pinMismatch = true;
                reply->abort();
            }
        });

    QNetworkReply* reply = method == QByteArrayLiteral("GET")
        ? manager.get(networkRequest)
        : manager.sendCustomRequest(networkRequest, method, body);
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, reply, &QNetworkReply::abort);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(QCoreApplication::instance(),
                     &QCoreApplication::aboutToQuit,
                     &loop, &QEventLoop::quit);
    timer.start(kRequestTimeoutMs);
    loop.exec(QEventLoop::ExcludeUserInputEvents);
    timer.stop();

    result.status = reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();
    result.body = reply->readAll();
    result.peerCertificate = reply->sslConfiguration().peerCertificate();
    result.peerSpkiFingerprint = spkiFingerprint(result.peerCertificate);
    if (result.peerSpkiFingerprint.isEmpty()) {
        result.peerSpkiFingerprint = observedDuringHandshake;
    }
    result.identityChanged = pinMismatch
        || (!bootstrap && !expectedSpki.isEmpty()
            && result.peerSpkiFingerprint != expectedSpki);
    if (result.identityChanged) {
        result.error = QStringLiteral(
            "Beacon identity changed — re-pair required");
    } else if (reply->error() == QNetworkReply::OperationCanceledError) {
        result.error = QStringLiteral("Beacon request timed out");
    } else if (reply->error() != QNetworkReply::NoError
               && result.status == 0) {
        result.error = reply->errorString();
    } else if (bootstrap && result.peerSpkiFingerprint.size() != 32) {
        result.error = QStringLiteral("Beacon did not present a usable identity");
    }
    reply->deleteLater();
    return result;
}

QString responseError(const HttpResult& result, const QString& fallback)
{
    if (!result.error.isEmpty()) return result.error;
    const QJsonDocument document = QJsonDocument::fromJson(result.body);
    if (document.isObject()) {
        const QJsonObject object = document.object();
        const QString detail = object.value(QStringLiteral("reason")).toString(
            object.value(QStringLiteral("error")).toString());
        if (!detail.isEmpty()) return detail;
    }
    return fallback + QStringLiteral(" (HTTP %1)").arg(result.status);
}

class PairBeaconTask : public QObject, public QRunnable
{
    Q_OBJECT

public:
    PairBeaconTask(QString url, QByteArray code, QString name)
        : m_BaseUrl(canonicalBaseUrl(std::move(url)))
        , m_Code(std::move(code))
        , m_Name(std::move(name))
    {
    }

    void run() override
    {
        const HttpResult pairing = request(
            apiUrl(m_BaseUrl, QStringLiteral("/pairing")),
            QByteArrayLiteral("GET"), {}, {}, true);
        if (!pairing.ok()) {
            emit finished(false,
                          responseError(pairing,
                                        QStringLiteral("Could not open Beacon pairing")),
                          {});
            return;
        }
        const QJsonDocument pairingDocument =
            QJsonDocument::fromJson(pairing.body);
        if (!pairingDocument.isObject()) {
            emit finished(false, QStringLiteral("Beacon pairing response is invalid"), {});
            return;
        }
        const QJsonObject pairingObject = pairingDocument.object();
        const QUuid beaconId(
            pairingObject.value(QStringLiteral("beacon_id")).toString());
        const QUuid pairingId(
            pairingObject.value(QStringLiteral("pairing_id")).toString());
        if (beaconId.isNull() || pairingId.isNull()
                || pairingObject.value(QStringLiteral("ciphersuite")).toString()
                    != QLatin1String("SPAKE2-P256-SHA256-HKDF-HMAC")) {
            emit finished(false,
                          QStringLiteral("Beacon pairing contract is incompatible"),
                          {});
            return;
        }
        if (!matchesSha256Fingerprint(
                    pairingObject.value(QStringLiteral("beacon_fingerprint")),
                    pairing.peerSpkiFingerprint)) {
            // The Beacon's own claimed identity disagrees with (or is
            // malformed relative to) what its TLS certificate actually
            // presented on this connection -- refuse rather than trust
            // either value alone.
            emit finished(false,
                          QStringLiteral("Beacon pairing response identity does not match its certificate"),
                          {});
            return;
        }

        const QByteArray clientSpki = spkiFingerprint(clientCertificate());
        BeaconSpake2Client spake(
            beaconId, pairingId, m_Code, clientSpki,
            pairing.peerSpkiFingerprint);
        QByteArray clientShare;
        QString cryptoError;
        if (!spake.begin(&clientShare, &cryptoError)) {
            emit finished(false, cryptoError, {});
            return;
        }

        const QByteArray startBody = QJsonDocument(QJsonObject{
            {QStringLiteral("client_share"),
             QString::fromLatin1(clientShare.toBase64())},
        }).toJson(QJsonDocument::Compact);
        const HttpResult start = request(
            apiUrl(m_BaseUrl,
                   QStringLiteral("/pairing/%1/spake2/start")
                       .arg(pairingId.toString(QUuid::WithoutBraces))),
            QByteArrayLiteral("POST"), startBody,
            pairing.peerSpkiFingerprint, false);
        if (!start.ok()) {
            emit finished(false,
                          responseError(start,
                                        QStringLiteral("Beacon rejected pairing share")),
                          {});
            return;
        }
        const QJsonObject startObject =
            QJsonDocument::fromJson(start.body).object();
        const QByteArray beaconShare = QByteArray::fromBase64(
            startObject.value(QStringLiteral("beacon_share"))
                .toString().toLatin1());
        const QByteArray beaconConfirmation = QByteArray::fromBase64(
            startObject.value(QStringLiteral("beacon_confirm"))
                .toString().toLatin1());
        QByteArray clientConfirmation;
        if (!spake.finish(beaconShare, beaconConfirmation,
                          &clientConfirmation, &cryptoError)) {
            emit finished(false, cryptoError, {});
            return;
        }

        const QByteArray confirmBody = QJsonDocument(QJsonObject{
            {QStringLiteral("client_confirm"),
             QString::fromLatin1(clientConfirmation.toBase64())},
        }).toJson(QJsonDocument::Compact);
        const HttpResult confirm = request(
            apiUrl(m_BaseUrl,
                   QStringLiteral("/pairing/%1/spake2/confirm")
                       .arg(pairingId.toString(QUuid::WithoutBraces))),
            QByteArrayLiteral("POST"), confirmBody,
            pairing.peerSpkiFingerprint, false);
        const QJsonObject confirmObject =
            QJsonDocument::fromJson(confirm.body).object();
        if (!confirm.ok()
                || confirmObject.value(QStringLiteral("status")).toString()
                    != QLatin1String("authorized")
                || QUuid(confirmObject.value(QStringLiteral("beacon_id")).toString())
                    != beaconId
                || !matchesSha256Fingerprint(
                        confirmObject.value(QStringLiteral("authorized_client_fingerprint")),
                        clientSpki)) {
            emit finished(false,
                          responseError(confirm,
                                        QStringLiteral("Beacon pairing confirmation failed")),
                          {});
            return;
        }

        QVariantMap record{
            {QStringLiteral("id"),
             beaconId.toString(QUuid::WithoutBraces).toLower()},
            {QStringLiteral("name"),
             m_Name.isEmpty() ? m_BaseUrl.host() : m_Name},
            {QStringLiteral("url"), m_BaseUrl.toString()},
            {QStringLiteral("spkiFingerprint"),
             QString::fromLatin1(pairing.peerSpkiFingerprint.toHex())},
            {QStringLiteral("identityState"), QStringLiteral("trusted")},
        };
        emit finished(true, QString(), record);
    }

signals:
    void finished(bool success, QString error, QVariantMap beacon);

private:
    QUrl m_BaseUrl;
    QByteArray m_Code;
    QString m_Name;
};

class RefreshBeaconHostsTask : public QObject, public QRunnable
{
    Q_OBJECT

public:
    RefreshBeaconHostsTask(QString id, QString url, QByteArray pin)
        : m_Id(std::move(id))
        , m_BaseUrl(canonicalBaseUrl(std::move(url)))
        , m_Pin(std::move(pin))
    {
    }

    void run() override
    {
        const HttpResult result = request(
            apiUrl(m_BaseUrl, QStringLiteral("/hosts")),
            QByteArrayLiteral("GET"), {}, m_Pin, false);
        if (!result.ok()) {
            emit finished(m_Id, {},
                          responseError(result,
                                        QStringLiteral("Could not read Beacon Hosts")),
                          result.identityChanged);
            return;
        }
        const QJsonDocument document = QJsonDocument::fromJson(result.body);
        if (!document.isArray()) {
            emit finished(m_Id, {},
                          QStringLiteral("Beacon Hosts response is invalid"), false);
            return;
        }
        emit finished(m_Id, document.array().toVariantList(), QString(), false);
    }

signals:
    void finished(QString beaconId,
                  QVariantList hosts,
                  QString error,
                  bool identityChanged);

private:
    QString m_Id;
    QUrl m_BaseUrl;
    QByteArray m_Pin;
};
}

BeaconManager* BeaconManager::s_Instance = nullptr;

BeaconManager* BeaconManager::get()
{
    if (s_Instance == nullptr) {
        s_Instance = new BeaconManager();
    }
    return s_Instance;
}

BeaconManager::BeaconManager(QObject* parent)
    : QObject(parent)
{
    loadCache();
    connect(this, &BeaconManager::beaconIdentityChanged,
            this, &BeaconManager::handleIdentityChanged,
            Qt::QueuedConnection);
    startDiscovery();
}

void BeaconManager::loadCache()
{
    SettingsDatabase* database = SettingsDatabase::get();
    if (database == nullptr || !database->isOpen()) return;

    QWriteLocker lock(&m_Lock);
    m_Beacons.clear();
    for (const QVariant& value : database->beacons()) {
        const QVariantMap row = value.toMap();
        BeaconRecord record;
        record.id = row.value(QStringLiteral("id")).toString();
        record.name = row.value(QStringLiteral("name")).toString();
        record.url = row.value(QStringLiteral("url")).toString();
        record.spkiFingerprint = QByteArray::fromHex(
            row.value(QStringLiteral("spkiFingerprint")).toByteArray());
        record.identityState =
            row.value(QStringLiteral("identityState")).toString();
        m_Beacons.insert(record.id, record);
    }
    m_Routes.clear();
    for (const QVariant& value : database->wakeRoutes()) {
        const QVariantMap row = value.toMap();
        WakeRoute route;
        route.provider = row.value(QStringLiteral("provider")).toString();
        route.beaconId = row.value(QStringLiteral("beaconId")).toString();
        route.beaconHostId =
            row.value(QStringLiteral("beaconHostId")).toString();
        m_Routes.insert(row.value(QStringLiteral("hostId")).toString(), route);
    }
}

QVariantMap BeaconManager::recordToVariant(const BeaconRecord& record)
{
    return {
        {QStringLiteral("id"), record.id},
        {QStringLiteral("name"), record.name},
        {QStringLiteral("url"), record.url},
        {QStringLiteral("spkiFingerprint"),
         QString::fromLatin1(record.spkiFingerprint.toHex())},
        {QStringLiteral("identityState"), record.identityState},
    };
}

QVariantList BeaconManager::pairedBeacons() const
{
    QReadLocker lock(&m_Lock);
    QVariantList rows;
    for (const BeaconRecord& record : m_Beacons) {
        rows.append(recordToVariant(record));
    }
    return rows;
}

QVariantList BeaconManager::discoveredBeacons() const
{
    QReadLocker lock(&m_Lock);
    QVariantList rows;
    for (const QVariantMap& row : m_Discovered) rows.append(row);
    return rows;
}

bool BeaconManager::pairing() const
{
    QReadLocker lock(&m_Lock);
    return m_Pairing;
}

void BeaconManager::pairBeacon(const QString& url,
                               const QString& shortCode,
                               const QString& displayName)
{
    if (shortCode.trimmed().isEmpty()) {
        emit pairingFinished(false, tr("Enter the Beacon pairing code."), {});
        return;
    }
    {
        QWriteLocker lock(&m_Lock);
        if (m_Pairing) {
            emit pairingFinished(false,
                                 tr("Another Beacon pairing is already running."),
                                 {});
            return;
        }
        m_Pairing = true;
    }
    emit pairingChanged();

    auto* task = new PairBeaconTask(
        url, shortCode.trimmed().toUtf8(), displayName.trimmed());
    connect(task, &PairBeaconTask::finished,
            this, &BeaconManager::handlePairingFinished);
    QThreadPool::globalInstance()->start(task);
}

void BeaconManager::handlePairingFinished(bool success,
                                          QString error,
                                          QVariantMap beacon)
{
    {
        QWriteLocker lock(&m_Lock);
        m_Pairing = false;
    }
    emit pairingChanged();

    QString beaconId;
    if (success) {
        SettingsDatabase* database = SettingsDatabase::get();
        beaconId = beacon.value(QStringLiteral("id")).toString();
        if (database == nullptr || !database->savePairedBeacon(
                beaconId,
                beacon.value(QStringLiteral("name")).toString(),
                beacon.value(QStringLiteral("url")).toString(),
                beacon.value(QStringLiteral("spkiFingerprint")).toString())) {
            success = false;
            error = tr("Could not save the paired Beacon.");
        } else {
            BeaconRecord record;
            record.id = beaconId;
            record.name = beacon.value(QStringLiteral("name")).toString();
            record.url = beacon.value(QStringLiteral("url")).toString();
            record.spkiFingerprint = QByteArray::fromHex(
                beacon.value(QStringLiteral("spkiFingerprint")).toByteArray());
            record.identityState = QStringLiteral("trusted");
            {
                QWriteLocker lock(&m_Lock);
                m_Beacons.insert(record.id, record);
            }
            emit beaconsChanged();
            refreshHosts(beaconId);
        }
    }
    emit pairingFinished(success, error, beaconId);
}

bool BeaconManager::removeBeacon(const QString& beaconId)
{
    SettingsDatabase* database = SettingsDatabase::get();
    if (database == nullptr || !database->removeBeacon(beaconId)) return false;
    {
        QWriteLocker lock(&m_Lock);
        m_Beacons.remove(beaconId);
        m_Hosts.remove(beaconId);
        for (auto it = m_Routes.begin(); it != m_Routes.end();) {
            if (it->beaconId == beaconId) it = m_Routes.erase(it);
            else ++it;
        }
    }
    emit beaconsChanged();
    return true;
}

void BeaconManager::refreshHosts(const QString& beaconId)
{
    BeaconRecord record;
    {
        QReadLocker lock(&m_Lock);
        record = m_Beacons.value(beaconId);
    }
    if (record.id.isEmpty() || record.identityState != QLatin1String("trusted")) {
        emit hostRefreshFailed(beaconId,
                               tr("Beacon must be trusted before reading Hosts."));
        return;
    }
    auto* task = new RefreshBeaconHostsTask(
        record.id, record.url, record.spkiFingerprint);
    connect(task, &RefreshBeaconHostsTask::finished,
            this, &BeaconManager::handleHostsFinished);
    QThreadPool::globalInstance()->start(task);
}

void BeaconManager::handleHostsFinished(QString beaconId,
                                        QVariantList hosts,
                                        QString error,
                                        bool identityChanged)
{
    if (identityChanged) {
        emit beaconIdentityChanged(beaconId);
        return;
    }
    if (!error.isEmpty()) {
        emit hostRefreshFailed(beaconId, error);
        return;
    }
    {
        QWriteLocker lock(&m_Lock);
        m_Hosts.insert(beaconId, hosts);
    }
    emit hostsChanged(beaconId);
}

QVariantList BeaconManager::hostsForBeacon(const QString& beaconId) const
{
    QReadLocker lock(&m_Lock);
    return m_Hosts.value(beaconId);
}

QVariantMap BeaconManager::wakeRouteForHost(const QString& hostId) const
{
    QReadLocker lock(&m_Lock);
    const WakeRoute route = m_Routes.value(hostId);
    return {
        {QStringLiteral("hostId"), hostId},
        {QStringLiteral("provider"), route.provider},
        {QStringLiteral("beaconId"), route.beaconId},
        {QStringLiteral("beaconHostId"), route.beaconHostId},
    };
}

bool BeaconManager::setDirectWake(const QString& hostId)
{
    SettingsDatabase* database = SettingsDatabase::get();
    if (database == nullptr
            || !database->setWakeRoute(hostId, QStringLiteral("direct"))) {
        return false;
    }
    {
        QWriteLocker lock(&m_Lock);
        m_Routes.insert(hostId, WakeRoute{});
    }
    return true;
}

bool BeaconManager::setBeaconWake(const QString& hostId,
                                  const QString& beaconId,
                                  const QString& beaconHostId)
{
    {
        QReadLocker lock(&m_Lock);
        const auto beacon = m_Beacons.constFind(beaconId);
        if (beacon == m_Beacons.constEnd()
                || beacon->identityState != QLatin1String("trusted")) {
            return false;
        }
    }
    SettingsDatabase* database = SettingsDatabase::get();
    if (database == nullptr || !database->setWakeRoute(
            hostId, QStringLiteral("beacon"), beaconId, beaconHostId)) {
        return false;
    }
    WakeRoute route;
    route.provider = QStringLiteral("beacon");
    route.beaconId = beaconId;
    route.beaconHostId = beaconHostId;
    {
        QWriteLocker lock(&m_Lock);
        m_Routes.insert(hostId, route);
    }
    return true;
}

bool BeaconManager::hasBeaconRoute(const QString& hostId) const
{
    QReadLocker lock(&m_Lock);
    const WakeRoute route = m_Routes.value(hostId);
    return route.provider == QLatin1String("beacon")
        && !route.beaconId.isEmpty()
        && !route.beaconHostId.isEmpty();
}

bool BeaconManager::dispatchWake(const QString& hostId,
                                 QString* wakeId,
                                 QString* error)
{
    WakeRoute route;
    BeaconRecord beacon;
    {
        QReadLocker lock(&m_Lock);
        route = m_Routes.value(hostId);
        beacon = m_Beacons.value(route.beaconId);
    }
    if (route.provider != QLatin1String("beacon")
            || route.beaconHostId.isEmpty() || beacon.id.isEmpty()) {
        if (error) *error = tr("No Beacon Wake route is configured for this Host.");
        return false;
    }
    if (beacon.identityState != QLatin1String("trusted")) {
        if (error) *error = tr("Beacon identity changed — re-pair required.");
        return false;
    }

    const QByteArray idempotency =
        QUuid::createUuid().toString(QUuid::WithoutBraces).toUtf8();
    const QString escapedHostId = QString::fromLatin1(
        QUrl::toPercentEncoding(route.beaconHostId));
    const HttpResult result = request(
        apiUrl(canonicalBaseUrl(beacon.url),
               QStringLiteral("/hosts/%1/wake").arg(escapedHostId)),
        QByteArrayLiteral("POST"), QByteArrayLiteral("{}"),
        beacon.spkiFingerprint, false,
        {{QByteArrayLiteral("Idempotency-Key"), idempotency}});
    if (result.identityChanged) {
        emit beaconIdentityChanged(beacon.id);
    }
    if (!result.ok() || result.status != 202) {
        if (error) {
            *error = responseError(result,
                                   tr("Beacon rejected the Wake request."));
        }
        return false;
    }
    const QJsonObject response = QJsonDocument::fromJson(result.body).object();
    const QString returnedWakeId =
        response.value(QStringLiteral("wake_id")).toString();
    if (returnedWakeId.isEmpty()
            || response.value(QStringLiteral("status")).toString()
                != QLatin1String("accepted")) {
        if (error) *error = tr("Beacon returned an invalid Wake receipt.");
        return false;
    }
    if (wakeId) *wakeId = returnedWakeId;
    if (error) error->clear();
    return true;
}

void BeaconManager::handleIdentityChanged(const QString& beaconId)
{
    SettingsDatabase* database = SettingsDatabase::get();
    if (database != nullptr) database->markBeaconIdentityChanged(beaconId);
    {
        QWriteLocker lock(&m_Lock);
        auto it = m_Beacons.find(beaconId);
        if (it != m_Beacons.end()) {
            it->identityState = QStringLiteral("changed");
        }
    }
    emit beaconsChanged();
}

void BeaconManager::startDiscovery()
{
    m_MdnsServer.reset(new QMdnsEngine::Server());
    m_MdnsBrowser = new QMdnsEngine::Browser(
        m_MdnsServer.data(), "_jochona-beacon._tcp.local.", nullptr, this);

    auto update = [this](const QMdnsEngine::Service& service) {
        const QMap<QByteArray, QByteArray> attributes = service.attributes();
        const QString id = QString::fromUtf8(attributes.value("id"));
        const QString fingerprint =
            QString::fromUtf8(attributes.value("fp")).toLower();
        if (id.isEmpty() || fingerprint.size() != 64
                || attributes.value("v") != QByteArrayLiteral("1")) {
            return;
        }
        QUrl url;
        url.setScheme(QStringLiteral("https"));
        url.setHost(QString::fromUtf8(service.hostname()));
        url.setPort(service.port());
        QVariantMap row{
            {QStringLiteral("id"), id},
            {QStringLiteral("name"), QString::fromUtf8(service.name())},
            {QStringLiteral("url"), url.toString()},
            {QStringLiteral("spkiFingerprintHint"), fingerprint},
        };
        {
            QWriteLocker lock(&m_Lock);
            m_Discovered.insert(id, row);
        }
        emit discoveredBeaconsChanged();
    };
    connect(m_MdnsBrowser, &QMdnsEngine::Browser::serviceAdded,
            this, update);
    connect(m_MdnsBrowser, &QMdnsEngine::Browser::serviceUpdated,
            this, update);
    connect(m_MdnsBrowser, &QMdnsEngine::Browser::serviceRemoved,
            this, [this](const QMdnsEngine::Service& service) {
        const QString id = QString::fromUtf8(
            service.attributes().value("id"));
        if (id.isEmpty()) return;
        {
            QWriteLocker lock(&m_Lock);
            m_Discovered.remove(id);
        }
        emit discoveredBeaconsChanged();
    });
}

#include "beaconmanager.moc"
