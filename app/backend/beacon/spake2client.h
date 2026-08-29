//
// SPDX-FileCopyrightText: Jochona Client Contributors
// SPDX-License-Identifier: GPL-3.0-only
//
#pragma once

#include <QByteArray>
#include <QString>
#include <QUuid>

// Client role A for the locked Beacon SPAKE2-P256-SHA256-HKDF-HMAC
// pairing contract. The short code is passed through scrypt and used only as
// the SPAKE2 scalar; it is never sent or reused as an application secret.
class BeaconSpake2Client
{
public:
    BeaconSpake2Client(QUuid beaconId,
                       QUuid pairingId,
                       QByteArray shortCode,
                       QByteArray clientSpkiSha256,
                       QByteArray beaconSpkiSha256);
    ~BeaconSpake2Client();

    // SHA-256 over the complete DER-encoded SubjectPublicKeyInfo sequence.
    // X509_pubkey_digest() hashes only the key bits and is not equivalent.
    static QByteArray certificateSpkiSha256(const QByteArray& certificateDer);

    // fixedScalar exists only for deterministic protocol-vector tests. Runtime
    // callers must leave it empty so OpenSSL generates a private random scalar.
    bool begin(QByteArray* clientShare,
               QString* error = nullptr,
               const QByteArray& fixedScalar = QByteArray());

    bool finish(const QByteArray& beaconShare,
                const QByteArray& beaconConfirmation,
                QByteArray* clientConfirmation,
                QString* error = nullptr);

private:
    void invalidate();

    QUuid m_BeaconId;
    QUuid m_PairingId;
    QByteArray m_ShortCode;
    QByteArray m_ClientSpkiSha256;
    QByteArray m_BeaconSpkiSha256;
    QByteArray m_X;
    QByteArray m_W;
    QByteArray m_ClientShare;
    bool m_Started = false;
};
