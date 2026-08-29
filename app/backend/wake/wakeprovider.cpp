//
// SPDX-FileCopyrightText: Jochona Client Contributors
// SPDX-License-Identifier: GPL-3.0-only
//
#include "wakeprovider.h"

#include "backend/beacon/beaconmanager.h"
#include "backend/nvcomputer.h"

#include <memory>

namespace
{
class DirectWakeProvider final : public IWakeProvider
{
public:
    QString name() const override { return QStringLiteral("Direct Wake"); }

    QVector<int> clientDispatchDelaysMs() const override
    {
        return {0, 1000, 3000};
    }

    bool send(const NvComputer& computer,
              QString* receipt,
              QString* error) const override
    {
        const bool sent = computer.wake();
        if (receipt) receipt->clear();
        if (error) {
            *error = sent ? QString()
                          : QStringLiteral("No Direct Wake datagram was sent");
        }
        return sent;
    }
};

class BeaconWakeProvider final : public IWakeProvider
{
public:
    QString name() const override { return QStringLiteral("Beacon"); }

    QVector<int> clientDispatchDelaysMs() const override
    {
        return {0};
    }

    bool send(const NvComputer& computer,
              QString* receipt,
              QString* error) const override
    {
        return BeaconManager::get()->dispatchWake(
            computer.uuid, receipt, error);
    }
};

std::unique_ptr<IWakeProvider> provider(const QString& hostId)
{
    if (BeaconManager::get()->hasBeaconRoute(hostId)) {
        return std::make_unique<BeaconWakeProvider>();
    }
    return std::make_unique<DirectWakeProvider>();
}
}

bool WakeProviderManager::canWake(const NvComputer& computer)
{
    return usesBeacon(computer.uuid) || canDirectWake(computer);
}

bool WakeProviderManager::canDirectWake(const NvComputer& computer)
{
    return !computer.manualMacAddress.isEmpty()
        || !computer.macAddress.isEmpty();
}

bool WakeProviderManager::usesBeacon(const QString& hostId)
{
    return BeaconManager::get()->hasBeaconRoute(hostId);
}

QString WakeProviderManager::providerName(const QString& hostId)
{
    return provider(hostId)->name();
}

QVector<int> WakeProviderManager::clientDispatchDelaysMs(
        const QString& hostId)
{
    return provider(hostId)->clientDispatchDelaysMs();
}

QVector<int> WakeProviderManager::directDispatchDelaysMs()
{
    return DirectWakeProvider().clientDispatchDelaysMs();
}

bool WakeProviderManager::send(const NvComputer& computer,
                               QString* receipt,
                               QString* error)
{
    return provider(computer.uuid)->send(computer, receipt, error);
}

bool WakeProviderManager::sendDirect(const NvComputer& computer,
                                     QString* receipt,
                                     QString* error)
{
    return DirectWakeProvider().send(computer, receipt, error);
}
