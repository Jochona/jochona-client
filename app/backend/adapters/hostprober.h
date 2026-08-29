//
// SPDX-FileCopyrightText: Lunaframe Client Contributors
//
// SPDX-License-Identifier: GPL-3.0-only
//
// Jochona: async per-host capability prober (proposal §4.4, §6.9). Runs the
// endpoint probe plan on a QThreadPool worker thread -- the same pattern as
// PendingPairingTask/PendingAddTask/PendingQuitTask in computermanager.cpp
// -- so HostAdapterManager (hostadaptermanager.h) never blocks the GUI
// thread waiting on a host that may not even exist anymore.
//
// HostProber is intentionally not built on NvHTTP: NvHTTP's request path is
// exception-based (GfeHttpResponseException/QtNetworkReplyException), which
// is the right model when a failed request means "abort the operation the
// user asked for", but wrong here, where a failed request just means
// "this host doesn't have that endpoint". The mTLS/SSL-pinning setup is
// copied from NvHTTP::openConnection (nvhttp.cpp) rather than shared,
// per the constraint that nvhttp.{h,cpp} stay untouched.
//
#pragma once

#include "hostcapabilities.h"
#include "backend/nvaddress.h"

#include <QObject>
#include <QRunnable>
#include <QSslCertificate>
#include <QSslError>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;

class HostProber : public QObject, public QRunnable
{
    Q_OBJECT

public:
    explicit HostProber(QString uuid, NvAddress address, uint16_t httpsPort, QSslCertificate serverCert);

    void run() override;

signals:
    // Emitted exactly once when the probe plan finishes, whether or not any
    // individual endpoint succeeded. capabilities.confidence tells the
    // caller how far the plan actually got.
    void capabilitiesReady(QString uuid, HostCapabilities capabilities);

private:
    struct ProbeResult
    {
        // present means the route handler is proven by 2xx, 401, 403, or
        // 405. responded distinguishes a definitive HTTP response, such as
        // 404, from a transport, TLS, or timeout failure. Probe completeness
        // depends on that distinction.
        bool present = false;
        bool responded = false;
        bool ok = false; // 2xx specifically
        int statusCode = 0;
        QByteArray body;
    };

    ProbeResult get(QNetworkAccessManager& nam, const QUrl& url, int timeoutMs) const;
    ProbeResult head(QNetworkAccessManager& nam, const QUrl& url, int timeoutMs) const;
    ProbeResult request(QNetworkAccessManager& nam, const QUrl& url, int timeoutMs, bool headOnly) const;

    void handleSslErrors(QNetworkReply* reply, const QList<QSslError>& errors) const;

    // Fills in permissions and the VirtualDisplay* bits from a /serverinfo
    // response body. Missing tags (plain Sunshine) leave the corresponding
    // field untouched.
    void parseServerInfo(const QByteArray& body, HostCapabilities& capabilities) const;

    QUrl m_BaseUrl;
    QString m_Uuid;
    NvAddress m_Address;
    uint16_t m_HttpsPort;
    QSslCertificate m_ServerCert;
};
