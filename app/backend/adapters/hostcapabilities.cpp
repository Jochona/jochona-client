//
// SPDX-FileCopyrightText: Lunaframe Client Contributors
//
// SPDX-License-Identifier: GPL-3.0-only
//
#include "hostcapabilities.h"

#include <QJsonArray>
#include <Limelight.h>
#include <QMetaEnum>
#include <utility>

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
        { HostCapabilities::JochonaManifest,            QLatin1String("jochonaManifest") },
    };
    // clang-format on
}

namespace
{
    QString manifestStatusName(HostCapabilities::ManifestStatus status)
    {
        switch (status) {
        case HostCapabilities::ManifestStatus::Invalid:
            return QStringLiteral("invalid");
        case HostCapabilities::ManifestStatus::Incompatible:
            return QStringLiteral("incompatible");
        case HostCapabilities::ManifestStatus::Compatible:
            return QStringLiteral("compatible");
        case HostCapabilities::ManifestStatus::Absent:
        default:
            return QStringLiteral("absent");
        }
    }

    HostCapabilities::ManifestStatus manifestStatusFromName(const QString& name)
    {
        if (name == QLatin1String("invalid")) {
            return HostCapabilities::ManifestStatus::Invalid;
        }
        if (name == QLatin1String("incompatible")) {
            return HostCapabilities::ManifestStatus::Incompatible;
        }
        if (name == QLatin1String("compatible")) {
            return HostCapabilities::ManifestStatus::Compatible;
        }
        return HostCapabilities::ManifestStatus::Absent;
    }
}

int
HostCapabilities::EncoderTuple::videoFormat() const
{
    const bool tenBit = bitDepth == 10;
    const bool yuv444 = chroma == QLatin1String("444");

    if (codec == QLatin1String("h264")) {
        if (tenBit) {
            return 0;
        }
        return yuv444 ? VIDEO_FORMAT_H264_HIGH8_444 : VIDEO_FORMAT_H264;
    }
    if (codec == QLatin1String("hevc") || codec == QLatin1String("h265")) {
        if (yuv444) {
            return tenBit ? VIDEO_FORMAT_H265_REXT10_444
                          : VIDEO_FORMAT_H265_REXT8_444;
        }
        return tenBit ? VIDEO_FORMAT_H265_MAIN10 : VIDEO_FORMAT_H265;
    }
    if (codec == QLatin1String("av1")) {
        if (yuv444) {
            return tenBit ? VIDEO_FORMAT_AV1_HIGH10_444
                          : VIDEO_FORMAT_AV1_HIGH8_444;
        }
        return tenBit ? VIDEO_FORMAT_AV1_MAIN10 : VIDEO_FORMAT_AV1_MAIN8;
    }
    return 0;
}

bool
HostCapabilities::EncoderTuple::supportsCapture(bool virtualDisplay) const
{
    return capture.contains(virtualDisplay ? QLatin1String("virtual")
                                           : QLatin1String("physical"));
}

QJsonObject
HostCapabilities::EncoderTuple::toJson() const
{
    QJsonArray captureArray;
    for (const QString& path : capture) {
        captureArray.append(path);
    }
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("codec"), codec},
        {QStringLiteral("profile"), profile},
        {QStringLiteral("bitDepth"), bitDepth},
        {QStringLiteral("chroma"), chroma},
        {QStringLiteral("width"), width},
        {QStringLiteral("height"), height},
        {QStringLiteral("fps"), fps},
        {QStringLiteral("hdr"), hdr},
        {QStringLiteral("capture"), captureArray},
    };
}

HostCapabilities::EncoderTuple
HostCapabilities::EncoderTuple::fromJson(const QJsonObject& object, bool* ok)
{
    EncoderTuple tuple;
    tuple.id = object.value(QStringLiteral("id")).toString();
    tuple.codec = object.value(QStringLiteral("codec")).toString().toLower();
    tuple.profile = object.value(QStringLiteral("profile")).toString().toLower();
    tuple.bitDepth = object.value(QStringLiteral("bitDepth")).toInt();
    tuple.chroma = object.value(QStringLiteral("chroma")).toString();
    tuple.width = object.value(QStringLiteral("width")).toInt();
    tuple.height = object.value(QStringLiteral("height")).toInt();
    tuple.fps = object.value(QStringLiteral("fps")).toInt();
    const QJsonValue hdrValue = object.value(QStringLiteral("hdr"));
    tuple.hdr = hdrValue.isObject()
            ? hdrValue.toObject().value(QStringLiteral("supported")).toBool()
            : hdrValue.toBool();
    for (const QJsonValue& path : object.value(QStringLiteral("capture")).toArray()) {
        const QString name = path.toString().toLower();
        if (!name.isEmpty() && !tuple.capture.contains(name)) {
            tuple.capture.append(name);
        }
    }

    const bool valid = !tuple.id.isEmpty()
            && (tuple.bitDepth == 8 || tuple.bitDepth == 10)
            && (tuple.chroma == QLatin1String("420")
                || tuple.chroma == QLatin1String("444"))
            && tuple.width > 0 && tuple.height > 0 && tuple.fps > 0
            && !tuple.capture.isEmpty() && tuple.videoFormat() != 0;
    if (ok != nullptr) {
        *ok = valid;
    }
    return tuple;
}

bool
HostCapabilities::EncoderTuple::operator==(const EncoderTuple& other) const
{
    return id == other.id
            && codec == other.codec
            && profile == other.profile
            && bitDepth == other.bitDepth
            && chroma == other.chroma
            && width == other.width
            && height == other.height
            && fps == other.fps
            && hdr == other.hdr
            && capture == other.capture;
}

HostCapabilities::ManifestStatus
HostCapabilities::applyJochonaManifest(const QJsonObject& object,
                                       const QString& expectedIdentity,
                                       QString* error)
{
    auto fail = [this, error](ManifestStatus status, const QString& message) {
        manifestStatus = status;
        if (error != nullptr) {
            *error = message;
        }
        return status;
    };

    const QJsonObject schema = object.value(QStringLiteral("schema")).toObject();
    if (!schema.value(QStringLiteral("major")).isDouble()
            || !schema.value(QStringLiteral("minor")).isDouble()) {
        return fail(ManifestStatus::Invalid,
                    QStringLiteral("Missing numeric schema major/minor"));
    }
    schemaMajor = schema.value(QStringLiteral("major")).toInt();
    schemaMinor = schema.value(QStringLiteral("minor")).toInt();
    family = Family::Jochona;
    if (schemaMajor != 1) {
        return fail(ManifestStatus::Incompatible,
                    QStringLiteral("Unsupported Jochona manifest major version %1")
                        .arg(schemaMajor));
    }

    const QJsonObject host = object.value(QStringLiteral("host")).toObject();
    hostSoftware = host.value(QStringLiteral("software")).toString();
    hostBuild = host.value(QStringLiteral("build")).toString();
    hostIdentity = host.value(QStringLiteral("identity")).toString();
    if (hostIdentity.isEmpty()) {
        return fail(ManifestStatus::Invalid,
                    QStringLiteral("Manifest host identity is missing"));
    }
    if (!expectedIdentity.isEmpty()
            && hostIdentity.compare(expectedIdentity, Qt::CaseInsensitive) != 0) {
        return fail(ManifestStatus::Invalid,
                    QStringLiteral("Manifest host identity does not match /serverinfo"));
    }

    const QJsonObject capacity =
        host.value(QStringLiteral("capacity")).toObject();
    capacityState = capacity.value(QStringLiteral("state")).toString();
    maxSessions = capacity.value(QStringLiteral("maxSessions")).toInt();
    activeApplication =
        capacity.value(QStringLiteral("activeApplication")).toString();
    if (capacityState != QLatin1String("ready")
            && capacityState != QLatin1String("busy")) {
        return fail(ManifestStatus::Invalid,
                    QStringLiteral("Manifest capacity state is invalid"));
    }
    if (maxSessions < 1) {
        return fail(ManifestStatus::Invalid,
                    QStringLiteral("Manifest maxSessions must be positive"));
    }

    permissionNames.clear();
    for (const QJsonValue& permission :
         object.value(QStringLiteral("permissions")).toArray()) {
        const QString name = permission.toString();
        if (!name.isEmpty() && !permissionNames.contains(name)) {
            permissionNames.append(name);
        }
    }
    if (permissionNames.contains(QLatin1String("session.launch"))) {
        permissions |= Permissions(ListApps | ViewStream | LaunchApps);
    }
    if (permissionNames.contains(QLatin1String("session.stop"))) {
        permissions |= LaunchApps;
    }

    QVector<EncoderTuple> parsedTuples;
    for (const QJsonValue& value :
         object.value(QStringLiteral("encoderTuples")).toArray()) {
        if (!value.isObject()) {
            return fail(ManifestStatus::Invalid,
                        QStringLiteral("Encoder tuple is not an object"));
        }
        bool tupleOk = false;
        const EncoderTuple tuple =
            EncoderTuple::fromJson(value.toObject(), &tupleOk);
        if (!tupleOk) {
            return fail(ManifestStatus::Invalid,
                        QStringLiteral("Encoder tuple is malformed"));
        }
        for (const EncoderTuple& existing : std::as_const(parsedTuples)) {
            if (existing.id == tuple.id) {
                return fail(ManifestStatus::Invalid,
                            QStringLiteral("Encoder tuple IDs are not unique"));
            }
        }
        parsedTuples.append(tuple);
    }
    encoderTuples = parsedTuples;

    const QJsonObject virtualDisplay =
        object.value(QStringLiteral("virtualDisplay")).toObject();
    if (virtualDisplay.value(QStringLiteral("installed")).toBool()) {
        capabilities |= VirtualDisplayCapable;
    }
    if (virtualDisplay.value(QStringLiteral("healthy")).toBool()) {
        capabilities |= VirtualDisplayDriverReady;
    }
    const QJsonObject hostVolume =
        object.value(QStringLiteral("runtimeControls")).toObject()
            .value(QStringLiteral("hostVolume")).toObject();
    if (hostVolume.value(QStringLiteral("available")).toBool()) {
        capabilities |= VolumeControl;
    }

    capabilities |= JochonaManifest;
    manifestStatus = ManifestStatus::Compatible;
    if (error != nullptr) {
        error->clear();
    }
    return manifestStatus;
}

QString
HostCapabilities::selectEncoderTuple(
        int requestedWidth,
        int requestedHeight,
        int requestedFps,
        const QList<int>& preferredVideoFormats,
        bool requestedHdr,
        bool virtualDisplay) const
{
    if (manifestStatus != ManifestStatus::Compatible) {
        return QString();
    }

    for (int videoFormat : preferredVideoFormats) {
        for (const EncoderTuple& tuple : encoderTuples) {
            if (tuple.width == requestedWidth
                    && tuple.height == requestedHeight
                    && tuple.fps == requestedFps
                    && tuple.hdr == requestedHdr
                    && tuple.videoFormat() == videoFormat
                    && tuple.supportsCapture(virtualDisplay)) {
                return tuple.id;
            }
        }
    }
    return QString();
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
    case Family::Jochona:
        return QStringLiteral("jochona");
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
    else if (name == QLatin1String("jochona")) {
        return Family::Jochona;
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
    object["manifestStatus"] = manifestStatusName(manifestStatus);
    object["schemaMajor"] = schemaMajor;
    object["schemaMinor"] = schemaMinor;
    object["hostSoftware"] = hostSoftware;
    object["hostBuild"] = hostBuild;
    object["hostIdentity"] = hostIdentity;
    object["capacityState"] = capacityState;
    object["maxSessions"] = maxSessions;
    object["activeApplication"] = activeApplication;

    QJsonArray manifestPermissions;
    for (const QString& permission : permissionNames) {
        manifestPermissions.append(permission);
    }
    object["permissionNames"] = manifestPermissions;

    QJsonArray tuples;
    for (const EncoderTuple& tuple : encoderTuples) {
        tuples.append(tuple.toJson());
    }
    object["encoderTuples"] = tuples;

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

    caps.manifestStatus = manifestStatusFromName(
        object.value(QStringLiteral("manifestStatus")).toString());
    caps.schemaMajor = object.value(QStringLiteral("schemaMajor")).toInt();
    caps.schemaMinor = object.value(QStringLiteral("schemaMinor")).toInt();
    caps.hostSoftware =
        object.value(QStringLiteral("hostSoftware")).toString();
    caps.hostBuild = object.value(QStringLiteral("hostBuild")).toString();
    caps.hostIdentity =
        object.value(QStringLiteral("hostIdentity")).toString();
    caps.capacityState =
        object.value(QStringLiteral("capacityState")).toString();
    caps.maxSessions =
        object.value(QStringLiteral("maxSessions")).toInt();
    caps.activeApplication =
        object.value(QStringLiteral("activeApplication")).toString();
    for (const QJsonValue& permission :
         object.value(QStringLiteral("permissionNames")).toArray()) {
        caps.permissionNames.append(permission.toString());
    }
    for (const QJsonValue& tuple :
         object.value(QStringLiteral("encoderTuples")).toArray()) {
        bool tupleOk = false;
        const EncoderTuple parsed =
            EncoderTuple::fromJson(tuple.toObject(), &tupleOk);
        if (tupleOk) {
            caps.encoderTuples.append(parsed);
        }
    }

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
            abrFeatures == other.abrFeatures &&
            manifestStatus == other.manifestStatus &&
            schemaMajor == other.schemaMajor &&
            schemaMinor == other.schemaMinor &&
            hostSoftware == other.hostSoftware &&
            hostBuild == other.hostBuild &&
            hostIdentity == other.hostIdentity &&
            capacityState == other.capacityState &&
            maxSessions == other.maxSessions &&
            activeApplication == other.activeApplication &&
            permissionNames == other.permissionNames &&
            encoderTuples == other.encoderTuples;
}

HostCapabilities
HostCapabilities::mergeProbeResult(const HostCapabilities& cached,
                                   const HostCapabilities& probed)
{
    if (cached.confidence == Confidence::Confirmed
            && probed.confidence != Confidence::Confirmed) {
        return cached;
    }
    return probed;
}
