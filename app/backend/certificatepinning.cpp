//
// SPDX-FileCopyrightText: Jochona Client Contributors
// SPDX-License-Identifier: GPL-3.0-only
//
#include "certificatepinning.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QSslConfiguration>
#include <QSslError>

void CertificatePinning::restrictTrustToPin(QSslConfiguration& config,
                                            const QSslCertificate& pinnedCert)
{
    // A self-signed certificate validates itself once it is the sole
    // trusted anchor, so this makes Qt's own chain validation reject
    // anything but the exact pin -- including a certificate that chains to
    // a real public root, which previously validated silently against the
    // system default CA store and never reached any pinning check at all.
    config.setCaCertificates(pinnedCert.isNull()
                                  ? QList<QSslCertificate>{}
                                  : QList<QSslCertificate>{pinnedCert});
}

CertificatePinning::Connections
CertificatePinning::install(QNetworkAccessManager* nam,
                            QObject* context,
                            QSslCertificate pinnedCert,
                            bool* mismatchedAfterHandshake)
{
    Connections connections;

    connections.sslErrors = QObject::connect(
        nam, &QNetworkAccessManager::sslErrors, context,
        [pinnedCert](QNetworkReply* reply, const QList<QSslError>& errors) {
            if (pinnedCert.isNull()) {
                // Never blindly trust a host with no pinned cert to compare against.
                return;
            }

            for (const QSslError& error : errors) {
                if (pinnedCert != error.certificate()) {
                    return;
                }
            }

            // Every reported chain-trust failure is attributable to the
            // exact certificate we already pinned (self-signed/untrusted
            // root is the expected shape for a Host cert). Let the
            // handshake continue so encrypted() below can make the real
            // accept/reject decision before any request data is sent.
            reply->ignoreSslErrors(errors);
        });

    connections.encrypted = QObject::connect(
        nam, &QNetworkAccessManager::encrypted, context,
        [pinnedCert, mismatchedAfterHandshake](QNetworkReply* reply) {
            // Fires once the TLS handshake completes and before any HTTP
            // request data leaves the socket. Aborting here stops a
            // publicly CA-trusted impostor certificate -- which never
            // raises sslErrors above -- just as reliably as it stops an
            // untrusted self-signed one.
            if (pinnedCert.isNull()
                    || reply->sslConfiguration().peerCertificate() != pinnedCert) {
                if (mismatchedAfterHandshake) {
                    *mismatchedAfterHandshake = true;
                }
                reply->abort();
            }
        });

    return connections;
}

void CertificatePinning::uninstall(const Connections& connections)
{
    QObject::disconnect(connections.sslErrors);
    QObject::disconnect(connections.encrypted);
}
