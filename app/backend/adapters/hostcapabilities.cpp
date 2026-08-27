//
// SPDX-FileCopyrightText: Lunaframe Client Contributors
//
// SPDX-License-Identifier: GPL-3.0-only
//
#include "hostcapabilities.h"

#include <QJsonArray>
#include <QMetaEnum>

namespace
{
    struct CapabilityName
    {
        HostCapabilities::Capability capability;
        QLatin1String name;
    };

    // clang-format off
    const CapabilityName kCapabilityNames[] = {
        { HostCapabilities::RunningAppState,           QLatin1String("runningAppState") },
        { HostCapabilities::Clipboard,                 QLatin1String("clipboard") },
        { HostCapabilities::VirtualDisplayCapable,     QLatin1String("virtualDisplayCapable") },
        { HostCapabilities::VirtualDisplayDriverReady, QLatin1String("virtualDisplayDriverReady") },
        { HostCapabilities::DisplayModes,              QLatin1String("displayModes") },
        { HostCapabilities::ServerResolution,          QLatin1String("serverResolution") },
        { HostCapabilities::ServerAudio,               QLatin1String("serverAudio") },
        { HostCapabilities::ClientAudio,               QLatin1String("clientAudio") },
        { HostCapabilities::VolumeControl,             QLatin1String("volumeControl") },
        { HostCapabilities::ActionToggle,              QLatin1String("actionToggle") },
        { HostCapabilities::ActionCancel,              QLatin1String("actionCancel") },
        { HostCapabilities::ActionBitrates,            QLatin1String("actionBitrates") },
        { HostCapabilities::RuntimeBitrate,            QLatin1String("runtimeBitrate") },
    };
    // clang-format on
}

QString
HostCapabilities::familyName(Family family)
{
    switch (family) {
    case Family::Sunshine:
        return QStringLiteral("sunshine");
    case Family::Apollo:
        return QStringLiteral("apollo");
    case Family::Vibepollo:
        return QStringLiteral("vibepollo");
    case Family::Unknown:
    default:
        return QStringLiteral("unknown");
    }
}

HostCapabilities::Family
HostCapabilities::familyFromName(const QString& name)
{
    if (name == QLatin1String("sunshine")) {
        return Family::Sunshine;
    }
    else if (name == QLatin1String("apollo")) {
        return Family::Apollo;
    }
    else if (name == QLatin1String("vibepollo")) {
        return Family::Vibepollo;
    }
    else {
        return Family::Unknown;
    }
}

QString
HostCapabilities::confidenceName(Confidence confidence)
{
    switch (confidence) {
    case Confidence::Partial:
        return QStringLiteral("partial");
    case Confidence::Confirmed:
        return QStringLiteral("confirmed");
    case Confidence::Unknown:
    default:
        return QStringLiteral("unknown");
    }
}

HostCapabilities::Confidence
HostCapabilities::confidenceFromName(const QString& name)
{
    if (name == QLatin1String("partial")) {
        return Confidence::Partial;
    }
    else if (name == QLatin1String("confirmed")) {
        return Confidence::Confirmed;
    }
    else {
        return Confidence::Unknown;
    }
}

QJsonObject
HostCapabilities::toJson() const
{
    QJsonObject object;

    object["family"] = familyName(family);
    object["confidence"] = confidenceName(confidence);
    object["permissionMask"] = static_cast<qint64>(static_cast<quint32>(permissions));
    object["lastProbed"] = lastProbed.isValid() ? lastProbed.toString(Qt::ISODateWithMs) : QString();
    object["abrVersion"] = abrVersion;

    QJsonArray features;
    for (const QString& feature : abrFeatures) {
        features.append(feature);
    }
    object["abrFeatures"] = features;

    QJsonArray capabilityNames;
    for (const CapabilityName& entry : kCapabilityNames) {
        if (hasCapability(entry.capability)) {
            capabilityNames.append(entry.name);
        }
    }
    object["capabilities"] = capabilityNames;

    return object;
}

HostCapabilities
HostCapabilities::fromJson(const QJsonObject& object)
{
    HostCapabilities caps;

    caps.family = familyFromName(object.value("family").toString());
    caps.confidence = confidenceFromName(object.value("confidence").toString());
    caps.permissions = Permissions(Permission(static_cast<quint32>(object.value("permissionMask").toDouble(0))));

    const QString lastProbedString = object.value("lastProbed").toString();
    if (!lastProbedString.isEmpty()) {
        caps.lastProbed = QDateTime::fromString(lastProbedString, Qt::ISODateWithMs);
    }

    caps.abrVersion = object.value("abrVersion").toInt();

    for (const QJsonValue& feature : object.value("abrFeatures").toArray()) {
        caps.abrFeatures.append(feature.toString());
    }

    Capabilities capabilities = NoCapabilities;
    for (const QJsonValue& name : object.value("capabilities").toArray()) {
        const QString capabilityName = name.toString();
        for (const CapabilityName& entry : kCapabilityNames) {
            if (entry.name == capabilityName) {
                capabilities |= entry.capability;
                break;
            }
        }
    }
    caps.capabilities = capabilities;

    return caps;
}

bool
HostCapabilities::operator==(const HostCapabilities& other) const
{
    return family == other.family &&
            capabilities == other.capabilities &&
            permissions == other.permissions &&
            confidence == other.confidence &&
            lastProbed == other.lastProbed &&
            abrVersion == other.abrVersion &&
            abrFeatures == other.abrFeatures;
}
