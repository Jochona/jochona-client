//
// SPDX-FileCopyrightText: Jochona Client Contributors
// SPDX-License-Identifier: GPL-3.0-only
//
// Jochona: one reusable peer-certificate pinning path shared by every
// direct-to-Host mTLS caller (NvHTTP, HostProber). ADR-0007 requires Host
// Identity to be authenticated by its pinned certificate; a naive
// implementation that only compares the pin inside a QNetworkAccessManager
// sslErrors handler is unsound, because sslErrors fires only when Qt's own
// CA-chain validation *fails*. A certificate that happens to chain to a
// publicly trusted root (for the requested hostname) never raises
// sslErrors at all, so that comparison is silently skipped and the request
// -- including its body -- is sent to whatever server presented that
// cert, before anyone checked it against the pin.
//
// Two independent layers close that gap:
//
// 1. restrictTrustToPin() replaces the request's trusted CA list with
//    exactly the pinned certificate before the request is ever sent
//    (QSslConfiguration::setCaCertificates({pin})). A self-signed pinned
//    certificate validates itself once it is the sole trusted anchor, so a
//    peer presenting anything else -- including a certificate that chains
//    to a real public root -- now fails Qt's own chain validation and
//    raises sslErrors, instead of silently passing. This layer does not
//    depend on any particular Qt signal firing.
// 2. install() makes the real accept/reject decision once, synchronously,
//    inside QNetworkAccessManager::encrypted(), which Qt documents as
//    firing after the TLS handshake completes but before any HTTP request
//    data (headers or body) is transmitted -- aborting the reply there
//    stops transmission regardless of chain-trust outcome. Qt's own docs
//    only guarantee `encrypted()` for the first connection to a given
//    host:port; a connection the NAM reuses may not re-emit it. Callers
//    MUST NOT rely on layer 2 alone for that reason -- restrictTrustToPin()
//    is the connection-reuse-independent backstop, and both layers must be
//    applied together.
//
// sslErrors is still handled by install(), but only to let Qt continue
// past its own chain-trust failure for a certificate that (thanks to
// restrictTrustToPin()) can now *only* fail because it does not match the
// exact pin -- it is no longer where the trust decision itself is made.
//
#pragma once

#include <QList>
#include <QMetaObject>
#include <QSslCertificate>
#include <QSslConfiguration>

class QNetworkAccessManager;
class QObject;

class CertificatePinning
{
public:
    // Layer 1 (see file header): restricts config's trusted CA list to
    // exactly pinnedCert, so only a peer presenting that precise
    // certificate can complete TLS validation at all -- independent of
    // whether encrypted() fires for this particular connection. Call this
    // on the QSslConfiguration BEFORE attaching it to the QNetworkRequest
    // that is about to be sent. A null pinnedCert clears the CA list
    // entirely (trust nothing) rather than falling back to the system
    // default store.
    static void restrictTrustToPin(QSslConfiguration& config,
                                    const QSslCertificate& pinnedCert);

    // Installs the sslErrors + encrypted handlers on nam for the lifetime
    // of one request. context owns the connections (pass the caller, e.g.
    // "this"); the caller must uninstall() with the returned handle once
    // the request has finished so a reused QNetworkAccessManager doesn't
    // accumulate handlers across requests with different pinned certs.
    //
    // pinnedCert is compared by full certificate equality (matching the
    // pinning semantics NvHTTP and HostProber already used), not just the
    // public key -- rotating the Host's certificate is an Identity Changed
    // event handled above this layer, not a transparent continuation.
    //
    // mismatchedAfterHandshake, when non-null, is set to true the moment
    // encrypted() aborts the reply for a certificate mismatch. It must
    // outlive the synchronous QEventLoop the caller runs to drive the
    // request, since Qt invokes the handler from within that loop. Callers
    // that want a precise "identity mismatch" error (instead of the reply
    // reporting a generic OperationCanceledError from the abort) should
    // check this flag before interpreting reply->error().
    struct Connections
    {
        QMetaObject::Connection sslErrors;
        QMetaObject::Connection encrypted;
    };

    static Connections install(QNetworkAccessManager* nam,
                                QObject* context,
                                QSslCertificate pinnedCert,
                                bool* mismatchedAfterHandshake = nullptr);

    static void uninstall(const Connections& connections);
};
