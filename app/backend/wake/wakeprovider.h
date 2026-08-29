//
// SPDX-FileCopyrightText: Jochona Client Contributors
// SPDX-License-Identifier: GPL-3.0-only
//
#pragma once

#include <QString>
#include <QVector>

class NvComputer;

// A Wake Provider owns the egress semantics for one user Wake action. Direct
// Wake asks the Client to schedule its three local datagrams; Beacon accepts
// one authenticated request and owns the LAN-side burst itself.
class IWakeProvider
{
public:
    virtual ~IWakeProvider() = default;
    virtual QString name() const = 0;
    virtual QVector<int> clientDispatchDelaysMs() const = 0;
    virtual bool send(const NvComputer& computer,
                      QString* receipt,
                      QString* error) const = 0;
};

class WakeProviderManager
{
public:
    static bool canWake(const NvComputer& computer);
    static bool canDirectWake(const NvComputer& computer);
    static bool usesBeacon(const QString& hostId);
    static QString providerName(const QString& hostId);
    static QVector<int> clientDispatchDelaysMs(const QString& hostId);
    static QVector<int> directDispatchDelaysMs();
    static bool send(const NvComputer& computer,
                     QString* receipt = nullptr,
                     QString* error = nullptr);
    static bool sendDirect(const NvComputer& computer,
                           QString* receipt = nullptr,
                           QString* error = nullptr);
};
