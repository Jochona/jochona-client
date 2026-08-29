#include "negotiator.h"

#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScreen>
#include <QVector>

#include "backend/computermanager.h"
#include "backend/nvcomputer.h"
#include "backend/systemproperties.h"
#include "core/settingsdatabase.h"
#include "settings/streamingpreferences.h"
#include "settings/effectivesettingsresolver.h"

#include <Limelight.h> // SCM_* codec bits

SystemProperties* Negotiator::s_Properties = nullptr;
ComputerManager* Negotiator::s_Manager = nullptr;

void
Negotiator::setSystemProperties(SystemProperties* properties)
{
    s_Properties = properties;
}

void
Negotiator::setComputerManager(ComputerManager* manager)
{
    s_Manager = manager;
}

Negotiator*
Negotiator::get()
{
    static Negotiator* instance = new Negotiator();
    return instance;
}

Negotiator::Negotiator()
{
    // Screen hotplug changes the device profile
    connect(qApp, &QGuiApplication::screenAdded, this, &Negotiator::handleScreenCountChanged);
    connect(qApp, &QGuiApplication::screenRemoved, this, &Negotiator::handleScreenCountChanged);
}

void
Negotiator::handleScreenCountChanged()
{
    emit deviceProfileChanged();
}

static QVariantMap screenToMap(QScreen* screen)
{
    QVariantMap map;
    const QRect geometry = screen->geometry();
    const int maxRefresh = qRound(screen->refreshRate());

    // No released Qt exposes an HDR-capability query on QScreen; hdr stays
    // false and deviceProfile documents it as best-effort. A real signal can
    // come from the platform backends later.
    bool hdr = false;

    map.insert("name", screen->name());
    map.insert("width", geometry.width());
    map.insert("height", geometry.height());
    map.insert("maxRefresh", maxRefresh);
    map.insert("hdr", hdr);
    map.insert("primary", screen == QGuiApplication::primaryScreen());
    return map;
}

QList<QVariantMap>
Negotiator::displays() const
{
    QList<QVariantMap> displays;
    const QList<QScreen*> screens = QGuiApplication::screens();
    for (QScreen* screen : screens) {
        displays.append(screenToMap(screen));
    }
    return displays;
}

QVariantMap
Negotiator::deviceProfile() const
{
    QVariantMap profile;
    QScreen* primary = QGuiApplication::primaryScreen();
    if (primary) {
        profile = screenToMap(primary);
    }

    // Decoder side: SystemProperties ran the SDL hardware decode test
    if (s_Properties != nullptr) {
        profile.insert("hardwareDecoder", s_Properties->hwAccelerationAvailable());
        profile.insert("maxStreamWidth", s_Properties->maxDecoderResolution().width());
        profile.insert("maxStreamHeight", s_Properties->maxDecoderResolution().height());
        profile.insert("decoderSupportsHdr", s_Properties->hdrCapable());
    }
    return profile;
}

QVariantMap
Negotiator::loadOverridesFromDatabase(const QString& uuid) const
{
    SettingsDatabase* database = SettingsDatabase::get();
    if (database == nullptr || !database->isOpen() || uuid.isEmpty()) {
        return {};
    }
    const QString pairKey =
        EffectiveSettingsResolver::get()->clientDeviceId()
        + QLatin1Char('|') + uuid;
    QVariantMap bundle = database->settingsPatch(
        QStringLiteral("host_client_pair"), pairKey);
    QVariantMap values =
        bundle.value(QStringLiteral("values")).toMap();
    if (values.isEmpty()) {
        const QVariant legacy =
            database->setting(QStringLiteral("quality_override.") + uuid);
        QJsonParseError error {};
        const QJsonDocument document = QJsonDocument::fromJson(
            legacy.toString().toUtf8(), &error);
        if (error.error == QJsonParseError::NoError
                && document.isObject()) {
            values = document.object().toVariantMap();
            database->setSettingsPatch(
                QStringLiteral("host_client_pair"), pairKey, values);
            database->setSetting(
                QStringLiteral("quality_override.") + uuid, QVariant());
        }
    }
    QVariantMap result;
    for (const QString& key
         : {QStringLiteral("width"), QStringLiteral("height"),
            QStringLiteral("fps")}) {
        if (values.contains(key)) result.insert(key, values.value(key));
    }
    if (values.contains(QStringLiteral("bitrateKbps"))) {
        result.insert(QStringLiteral("bitrateKbps"),
                      values.value(QStringLiteral("bitrateKbps")));
    } else if (values.contains(QStringLiteral("bitrate"))) {
        result.insert(QStringLiteral("bitrateKbps"),
                      values.value(QStringLiteral("bitrate")));
    }
    if (values.contains(QStringLiteral("codec"))) {
        result.insert(QStringLiteral("codec"),
                      values.value(QStringLiteral("codec")));
    } else if (values.contains(QStringLiteral("videocfg"))) {
        switch (values.value(QStringLiteral("videocfg")).toInt()) {
        case StreamingPreferences::VCC_FORCE_H264:
            result.insert(QStringLiteral("codec"), QStringLiteral("h264"));
            break;
        case StreamingPreferences::VCC_FORCE_HEVC:
            result.insert(QStringLiteral("codec"), QStringLiteral("hevc"));
            break;
        case StreamingPreferences::VCC_FORCE_AV1:
            result.insert(QStringLiteral("codec"), QStringLiteral("av1"));
            break;
        default:
            result.insert(QStringLiteral("codec"), QStringLiteral("auto"));
            break;
        }
    }
    return result;
}
void
Negotiator::saveOverridesToDatabase(const QString& uuid,
                                    const QVariantMap& quality)
{
    SettingsDatabase* database = SettingsDatabase::get();
    if (database == nullptr || !database->isOpen() || uuid.isEmpty()) return;
    const QString pairKey =
        EffectiveSettingsResolver::get()->clientDeviceId()
        + QLatin1Char('|') + uuid;
    const QVariantMap bundle = database->settingsPatch(
        QStringLiteral("host_client_pair"), pairKey);
    QVariantMap values = bundle.value(QStringLiteral("values")).toMap();
    for (const QString& key
         : {QStringLiteral("width"), QStringLiteral("height"),
            QStringLiteral("fps"), QStringLiteral("bitrate"),
            QStringLiteral("bitrateKbps"), QStringLiteral("videocfg"),
            QStringLiteral("codec")}) {
        values.remove(key);
    }
    for (auto it = quality.constBegin(); it != quality.constEnd(); ++it) {
        values.insert(it.key(), it.value());
    }
    database->setSettingsPatch(
        QStringLiteral("host_client_pair"), pairKey, values,
        bundle.value(QStringLiteral("pins")).toMap(),
        bundle.value(QStringLiteral("floors")).toMap());
    emit qualityOverridesChanged();
}

QVariantMap
Negotiator::qualityOverride(const QString& uuid) const
{
    return loadOverridesFromDatabase(uuid);
}

void
Negotiator::setQualityOverride(const QString& uuid, const QVariantMap& override_)
{
    QVariantMap sanitized;
    for (auto it = override_.begin(); it != override_.end(); ++it) {
        const QString& key = it.key();
        if ((key == "width" || key == "height" || key == "fps" || key == "bitrateKbps") &&
            it.value().canConvert<int>()) {
            if (it.value().toInt() > 0) {
                sanitized.insert(key, it.value().toInt());
            }
        } else if (key == "codec") {
            const QString codec = it.value().toString();
            if (codec == "auto" || codec == "h264" || codec == "hevc" || codec == "av1") {
                sanitized.insert(key, codec);
            }
        }
    }
    saveOverridesToDatabase(uuid, sanitized);
}

void
Negotiator::clearQualityOverride(const QString& uuid)
{
    saveOverridesToDatabase(uuid, {});
}

QVariantMap
Negotiator::effectiveQualityFor(const QString& uuid) const
{
    const QVariantMap resolved =
        EffectiveSettingsResolver::get()->resolve({
            {QStringLiteral("hostUuid"), uuid},
        });
    const QVariantMap values =
        resolved.value(QStringLiteral("values")).toMap();
    QString codec = QStringLiteral("auto");
    switch (values.value(QStringLiteral("videocfg")).toInt()) {
    case StreamingPreferences::VCC_FORCE_H264:
        codec = QStringLiteral("h264");
        break;
    case StreamingPreferences::VCC_FORCE_HEVC:
        codec = QStringLiteral("hevc");
        break;
    case StreamingPreferences::VCC_FORCE_AV1:
        codec = QStringLiteral("av1");
        break;
    default:
        break;
    }
    QVariantMap reasons;
    const QVariantMap provenance =
        resolved.value(QStringLiteral("provenance")).toMap();
    for (const QString& key
         : {QStringLiteral("width"), QStringLiteral("height"),
            QStringLiteral("fps"), QStringLiteral("bitrateKbps"),
            QStringLiteral("codec")}) {
        const QVariantMap field = provenance.value(key).toMap();
        if (field.value(QStringLiteral("source")).toString()
                == QStringLiteral("Capability Safety")) {
            reasons.insert(key, field.value(QStringLiteral("reason")));
        }
    }
    return {
        {QStringLiteral("width"), values.value(QStringLiteral("width"))},
        {QStringLiteral("height"), values.value(QStringLiteral("height"))},
        {QStringLiteral("fps"), values.value(QStringLiteral("fps"))},
        {QStringLiteral("bitrateKbps"),
         values.value(QStringLiteral("bitrate"))},
        {QStringLiteral("codec"), codec},
        {QStringLiteral("reasons"), reasons},
        {QStringLiteral("auto"), !reasons.isEmpty()},
    };
}

QVariantMap
Negotiator::clampQualityFor(const QString& uuid,
                            const QVariantMap& candidate) const
{
    QVariantMap result;
    QVariantMap reasons;

    SystemProperties* props = s_Properties;

    // --- Device constraints (primary screen + decoder test) ---
    QScreen* primary = QGuiApplication::primaryScreen();
    int deviceWidth = primary ? primary->geometry().width() : 1920;
    int deviceHeight = primary ? primary->geometry().height() : 1080;
    int deviceRefresh = primary ? qRound(primary->refreshRate()) : 60;

    // The decoder test's maximum resolution is the honest ceiling; anything
    // smaller than a plausible mode means the async probe has not completed,
    // so don't clamp on it yet.
    const QSize decoderMax = props != nullptr ? props->maxDecoderResolution() : QSize();
    const bool decoderLimited = decoderMax.width() >= 640 && decoderMax.height() >= 480;

    // --- Host constraints (displayModes + codec bits) if known ---
    NvComputer* host = nullptr;
    const QVector<NvComputer*> hosts =
            s_Manager != nullptr ? s_Manager->getComputers() : QVector<NvComputer*>();
    for (NvComputer* candidate : hosts) {
        if (candidate->uuid == uuid) {
            host = candidate;
            break;
        }
    }

    int hostWidth = 0, hostHeight = 0, hostFps = 0;
    uint32_t hostCodecBits = SCM_H264;
    const bool hostKnown = host != nullptr;
    if (hostKnown) {
        for (const NvDisplayMode& mode : host->displayModes) {
            hostWidth = qMax(hostWidth, mode.width);
            hostHeight = qMax(hostHeight, mode.height);
            hostFps = qMax(hostFps, mode.refreshRate);
        }
        hostCodecBits = (uint32_t)host->serverCodecModeSupport;
    }
    // A host that has never been polled advertises no modes and no codec
    // bits. That is absence of information, not a zero-capability device:
    // clamping the chain to 0x0/H.264 would be a lie, so treat it unknown.
    const bool hostLimitsKnown = hostKnown && hostWidth > 0;

    // --- Requested mode, then display/decoder/Host safety limits ---
    int width = candidate.value(QStringLiteral("width"), 0).toInt();
    int height = candidate.value(QStringLiteral("height"), 0).toInt();
    int fps = candidate.value(QStringLiteral("fps"), 0).toInt();
    QString widthReason = QStringLiteral("Requested %1").arg(width);
    QString heightReason = QStringLiteral("Requested %1").arg(height);
    QString fpsReason = QStringLiteral("Requested %1").arg(fps);

    if (width <= 0 || height <= 0 || fps <= 0) {
        width = deviceWidth;
        height = deviceHeight;
        fps = deviceRefresh;
        widthReason = QStringLiteral("Automatic: display is %1x%2")
                          .arg(deviceWidth).arg(deviceHeight);
        heightReason = widthReason;
        fpsReason = QStringLiteral("Automatic: display runs at %1 Hz")
                        .arg(deviceRefresh);
    }

    if (deviceRefresh > 0 && fps > deviceRefresh) {
        // Round down to a rate the panel can actually present
        int clamped = deviceRefresh;
        for (int candidate : {144, 120, 90, 72, 60, 50, 30}) {
            if (candidate <= deviceRefresh) {
                clamped = candidate;
                break;
            }
        }
        fps = clamped;
        fpsReason = QStringLiteral("Clamped: display tops out at %1 Hz").arg(deviceRefresh);
    }

    if (width > deviceWidth || height > deviceHeight) {
        // Scale down preserving aspect; the stream renders 1:1 on this display
        const qreal scale = qMin((qreal)deviceWidth / width, (qreal)deviceHeight / height);
        width = int(width * scale) & ~1;
        height = int(height * scale) & ~1;
        widthReason = QStringLiteral("Clamped: exceeds this display (%1x%2)").arg(deviceWidth).arg(deviceHeight);
        heightReason = widthReason;
    }

    if (decoderLimited && (width > decoderMax.width() || height > decoderMax.height())) {
        const qreal scale = qMin((qreal)decoderMax.width() / width, (qreal)decoderMax.height() / height);
        width = int(width * scale) & ~1;
        height = int(height * scale) & ~1;
        widthReason = QStringLiteral("Clamped: hardware decoder tested at %1x%2 max")
                          .arg(decoderMax.width()).arg(decoderMax.height());
        heightReason = widthReason;
    }

    if (hostLimitsKnown && (width > hostWidth || height > hostHeight)) {
        width = qMin(width, hostWidth) & ~1;
        height = qMin(height, hostHeight) & ~1;
        widthReason = QStringLiteral("Clamped: host advertises %1x%2 max").arg(hostWidth).arg(hostHeight);
        heightReason = widthReason;
    }
    if (hostLimitsKnown && hostFps > 0 && fps > hostFps) {
        fps = hostFps;
        fpsReason = QStringLiteral("Clamped: host advertises %1 Hz max").arg(hostFps);
    }
    if (!hostLimitsKnown) {
        widthReason += QStringLiteral("; host limits unknown (not yet polled)");
    }

    // --- Codec request intersected with Host support ---
    QString codec = candidate.value(QStringLiteral("codec"),
                                    QStringLiteral("auto")).toString().toLower();
    QString codecReason = QStringLiteral("Requested %1").arg(codec);

    if (hostLimitsKnown && codec != "auto") {
        const bool supported = (codec == "h264") ||
                               (codec == "hevc" && (hostCodecBits & (SCM_HEVC | SCM_HEVC_MAIN10))) ||
                               (codec == "av1" && (hostCodecBits & SCM_AV1_MAIN8));
        if (!supported) {
            // Let the session fall back via its own auto-negotiation
            codecReason = QStringLiteral("Downgraded to auto: host does not advertise %1").arg(codec);
            codec = "auto";
        }
    }

    // --- Bitrate is a launch value; Runtime ABR is a separate future sink ---
    int bitrate = candidate.value(QStringLiteral("bitrateKbps"), 0).toInt();
    QString bitrateReason = QStringLiteral("Requested %1 Kbps").arg(bitrate);
    if (bitrate <= 0) {
        bitrate = StreamingPreferences::getDefaultBitrate(width, height, fps, false);
        bitrateReason = QStringLiteral("Automatic for %1x%2@%3")
                            .arg(width).arg(height).arg(fps);
    }

    const bool autoAdjusted = widthReason.startsWith("Clamped") || heightReason.startsWith("Clamped") ||
                              fpsReason.startsWith("Clamped") || codecReason.startsWith("Downgraded") ||
                              bitrateReason.startsWith("Auto");

    result.insert("width", width);
    result.insert("height", height);
    result.insert("fps", fps);
    result.insert("bitrateKbps", bitrate);
    result.insert("codec", codec);
    result.insert("auto", autoAdjusted);
    reasons.insert("width", widthReason);
    reasons.insert("height", heightReason);
    reasons.insert("fps", fpsReason);
    reasons.insert("bitrateKbps", bitrateReason);
    reasons.insert("codec", codecReason);
    result.insert("reasons", reasons);
    return result;
}
