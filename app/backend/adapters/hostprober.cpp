//
// SPDX-FileCopyrightText: Lunaframe Client Contributors
//
// SPDX-License-Identifier: GPL-3.0-only
//
#include "hostprober.h"

#include "backend/identitymanager.h"
#include "backend/nvhttp.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

#define SERVERINFO_TIMEOUT_MS 3000
#define PROBE_TIMEOUT_MS 1500

HostProber::HostProber(QString uuid, NvAddress address, uint16_t httpsPort, QSslCertificate serverCert)
    : m_Uuid(std::move(uuid)),
      m_Address(std::move(address)),
      m_HttpsPort(httpsPort),
      m_ServerCert(std::move(serverCert))
{
    m_BaseUrl.setScheme(QStringLiteral("https"));
    m_BaseUrl.setHost(m_Address.address());
    m_BaseUrl.setPort(m_HttpsPort);

    // HostAdapterManager creates us on the GUI thread and pushes us onto
    // QThreadPool; the default true autoDelete matches PendingPairingTask
    // et al. in computermanager.cpp.
    setAutoDelete(true);
}

void
HostProber::run()
{
    HostCapabilities caps;
    caps.lastProbed = QDateTime::currentDateTimeUtc();

    if (m_ServerCert.isNull() || m_HttpsPort == 0 || m_Address.isNull()) {
        // Can't run an mTLS probe without a pinned cert and a paired port.
        // Report Unknown rather than guessing at a family.
        emit capabilitiesReady(m_Uuid, caps);
        return;
    }

    // One QNetworkAccessManager per probe run, created on this worker
    // thread (QNetworkAccessManager is bound to the thread that creates it).
    QNetworkAccessManager nam;
    QNetworkProxy noProxy(QNetworkProxy::NoProxy);
    nam.setProxy(noProxy);

    QUrl serverInfoUrl = m_BaseUrl;
    serverInfoUrl.setPath(QStringLiteral("/serverinfo"));
    const ProbeResult serverInfo = get(nam, serverInfoUrl, SERVERINFO_TIMEOUT_MS);
    if (!serverInfo.ok) {
        // Couldn't reach even the Sunshine baseline endpoint.
        emit capabilitiesReady(m_Uuid, caps);
        return;
    }

    parseServerInfo(serverInfo.body, caps);
    caps.family = HostCapabilities::Family::Sunshine;
    caps.confidence = HostCapabilities::Confidence::Partial;

    // Baseline Sunshine has no /state, no clipboard, and no bitrate
    // endpoints; a 404 here short-circuits every Apollo-only probe below.
    QUrl stateUrl = m_BaseUrl;
    stateUrl.setPath(QStringLiteral("/state"));
    const ProbeResult state = get(nam, stateUrl, PROBE_TIMEOUT_MS);
    if (!state.present) {
        caps.confidence = HostCapabilities::Confidence::Confirmed;
        emit capabilitiesReady(m_Uuid, caps);
        return;
    }

    caps.family = HostCapabilities::Family::Apollo;
    caps.capabilities |= HostCapabilities::RunningAppState;

    QUrl abrUrl = m_BaseUrl;
    abrUrl.setPath(QStringLiteral("/api/abr/capabilities"));
    const ProbeResult abr = get(nam, abrUrl, PROBE_TIMEOUT_MS);
    if (abr.ok) {
        QJsonParseError parseError {};
        const QJsonDocument doc = QJsonDocument::fromJson(abr.body, &parseError);
        if (parseError.error == QJsonParseError::NoError && doc.isObject() && doc.object().contains(QLatin1String("version"))) {
            const QJsonObject obj = doc.object();

            // Only Vibepollo serves this endpoint at all, but the presence
            // of a well-formed "version" field is the actual signal --
            // matches the "parses with version field" detection rule.
            caps.family = HostCapabilities::Family::Vibepollo;
            caps.abrVersion = obj.value(QLatin1String("version")).toInt();

            if (obj.value(QLatin1String("supported")).toBool(false)) {
                for (const QJsonValue& feature : obj.value(QLatin1String("features")).toArray()) {
                    caps.abrFeatures.append(feature.toString());
                }
                if (caps.abrFeatures.contains(QLatin1String("runtime_bitrate"))) {
                    caps.capabilities |= HostCapabilities::RuntimeBitrate;
                }
            }
        }
    }

    struct ActionProbe
    {
        const char* path;
        HostCapabilities::Capability capability;
        quint32 requiredPermissionMask; // 0 == ungated; OR of HostCapabilities::Permission bits
    };

    // clang-format off
    static const ActionProbe kActionProbes[] = {
        { "/actions/clipboard",  HostCapabilities::Clipboard,
          static_cast<quint32>(HostCapabilities::ClipboardRead) | static_cast<quint32>(HostCapabilities::ClipboardSet) },
        { "/actions/volumes",    HostCapabilities::VolumeControl,    0 },
        { "/actions/toggle",     HostCapabilities::ActionToggle,     0 },
        { "/actions/cancel",     HostCapabilities::ActionCancel,     static_cast<quint32>(HostCapabilities::LaunchApps) },
        { "/action/bitrates",    HostCapabilities::ActionBitrates,   0 },
        { "/serverdisplaymodes", HostCapabilities::DisplayModes,     0 },
        { "/serverresolution",   HostCapabilities::ServerResolution, 0 },
        { "/serveraudio",        HostCapabilities::ServerAudio,      0 },
        { "/clientaudio",        HostCapabilities::ClientAudio,      0 },
    };
    // clang-format on

    const quint32 grantedPermissions = static_cast<quint32>(caps.permissions);

    for (const ActionProbe& probe : kActionProbes) {
        QUrl url = m_BaseUrl;
        url.setPath(QString::fromLatin1(probe.path));

        const ProbeResult result = head(nam, url, PROBE_TIMEOUT_MS);
        if (!result.present) {
            continue;
        }

        if (probe.requiredPermissionMask != 0 && (grantedPermissions & probe.requiredPermissionMask) == 0) {
            // The route exists but the current client isn't allowed to use
            // it; don't advertise a capability the adapter can't exercise.
            continue;
        }

        caps.capabilities |= probe.capability;
    }

    caps.confidence = HostCapabilities::Confidence::Confirmed;
    emit capabilitiesReady(m_Uuid, caps);
}

void
HostProber::parseServerInfo(const QByteArray& body, HostCapabilities& capabilities) const
{
    const QString xml = QString::fromUtf8(body);

    bool permissionOk = false;
    const quint32 permissionValue = NvHTTP::getXmlString(xml, "Permission").toUInt(&permissionOk);
    if (permissionOk) {
        capabilities.permissions = HostCapabilities::Permissions(HostCapabilities::Permission(permissionValue));
    }

    const QString virtualDisplayCapable = NvHTTP::getXmlString(xml, "VirtualDisplayCapable");
    if (virtualDisplayCapable.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0 || virtualDisplayCapable == QLatin1String("1")) {
        capabilities.capabilities |= HostCapabilities::VirtualDisplayCapable;
    }

    const QString virtualDisplayDriverReady = NvHTTP::getXmlString(xml, "VirtualDisplayDriverReady");
    if (virtualDisplayDriverReady.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0 || virtualDisplayDriverReady == QLatin1String("1")) {
        capabilities.capabilities |= HostCapabilities::VirtualDisplayDriverReady;
    }
}

void
HostProber::handleSslErrors(QNetworkReply* reply, const QList<QSslError>& errors) const
{
    if (m_ServerCert.isNull()) {
        // No pinned cert to compare against; never blindly trust a host.
        return;
    }

    for (const QSslError& error : errors) {
        if (m_ServerCert != error.certificate()) {
            return;
        }
    }

    reply->ignoreSslErrors(errors);
}

HostProber::ProbeResult
HostProber::get(QNetworkAccessManager& nam, const QUrl& url, int timeoutMs) const
{
    return request(nam, url, timeoutMs, false);
}

HostProber::ProbeResult
HostProber::head(QNetworkAccessManager& nam, const QUrl& url, int timeoutMs) const
{
    return request(nam, url, timeoutMs, true);
}

HostProber::ProbeResult
HostProber::request(QNetworkAccessManager& nam, const QUrl& url, int timeoutMs, bool headOnly) const
{
    ProbeResult result;

    QNetworkRequest netRequest(url);

    // Client-cert (mTLS) identity, same as NvHTTP::openConnection.
    netRequest.setSslConfiguration(IdentityManager::get()->getSslConfig());

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    netRequest.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
#endif
#if QT_VERSION >= QT_VERSION_CHECK(6, 3, 0)
    netRequest.setAttribute(QNetworkRequest::ConnectionCacheExpiryTimeoutSecondsAttribute, 0);
#endif

    auto sslErrorsConnection = connect(&nam, &QNetworkAccessManager::sslErrors, this,
                                        [this](QNetworkReply* reply, const QList<QSslError>& errors) {
                                            handleSslErrors(reply, errors);
                                        });

    QNetworkReply* reply = headOnly ? nam.head(netRequest) : nam.get(netRequest);

    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    if (QCoreApplication::instance() != nullptr) {
        connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, &loop, &QEventLoop::quit);
    }
    QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
    loop.exec(QEventLoop::ExcludeUserInputEvents);

    if (!reply->isFinished()) {
        reply->abort();
    }

    disconnect(sslErrorsConnection);

    result.statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() == QNetworkReply::NoError) {
        result.body = reply->readAll();
        result.ok = result.statusCode >= 200 && result.statusCode < 300;
        result.present = true;
    }
    else if (result.statusCode == 401 || result.statusCode == 403 || result.statusCode == 405) {
        // The route exists but is permission-gated or doesn't support this
        // method -- still tells us the host implements it.
        result.present = true;
    }
    // Anything else (404, connection refused, TLS failure, timeout) leaves
    // result.present == false.

    delete reply;
    return result;
}
