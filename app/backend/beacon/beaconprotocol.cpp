//
// SPDX-FileCopyrightText: Jochona Client Contributors
// SPDX-License-Identifier: GPL-3.0-only
//
#include "beaconprotocol.h"

#include <QLatin1String>
#include <QString>

bool matchesSha256Fingerprint(const QJsonValue& value, const QByteArray& expectedRaw)
{
    static const QLatin1String kPrefix("sha256:");
    const QString claimed = value.toString();
    if (expectedRaw.size() != 32 || !claimed.startsWith(kPrefix)) {
        return false;
    }
    const QString hex = claimed.mid(kPrefix.size());
    if (hex.size() != 64) {
        return false;
    }
    for (const QChar& c : hex) {
        if (!c.isDigit() && (c.toLower() < QLatin1Char('a') || c.toLower() > QLatin1Char('f'))) {
            return false;
        }
    }
    return QByteArray::fromHex(hex.toLatin1()) == expectedRaw;
}
