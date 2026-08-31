//
// SPDX-FileCopyrightText: Jochona Client Contributors
// SPDX-License-Identifier: GPL-3.0-only
//
// Jochona: small protocol-response validators shared by BeaconManager and
// its unit tests. Kept free of QNetworkAccessManager/mDNS dependencies so
// the crypto/protocol-correctness pieces stay testable without pulling in
// the rest of beaconmanager.cpp.
//
#pragma once

#include <QByteArray>
#include <QJsonValue>

// Validates a Beacon Client Protocol v1 JSON field of the form
// "sha256:<64-hex-chars>" (beacon_fingerprint / authorized_client_fingerprint
// in docs/protocols/client-v1.md) against a raw 32-byte SHA-256 fingerprint
// already observed independently (over TLS, or computed locally).
// Malformed values -- wrong prefix, wrong length, non-hex -- are rejected
// the same as a value mismatch.
bool matchesSha256Fingerprint(const QJsonValue& value, const QByteArray& expectedRaw);
