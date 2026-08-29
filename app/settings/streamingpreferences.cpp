#include "streamingpreferences.h"
#include "utils.h"
#include "core/settingsdatabase.h"

#include <QSettings>
#include <QTranslator>
#include <QCoreApplication>
#include <QLocale>
#include <QReadWriteLock>
#include <QtMath>

#include <QtDebug>

#define SER_STREAMSETTINGS "streamsettings"
#define SER_WIDTH "width"
#define SER_HEIGHT "height"
#define SER_FPS "fps"
#define SER_BITRATE "bitrate"
#define SER_UNLOCK_BITRATE "unlockbitrate"
#define SER_AUTOADJUSTBITRATE "autoadjustbitrate"
#define SER_FULLSCREEN "fullscreen"
#define SER_VSYNC "vsync"
#define SER_GAMEOPTS "gameopts"
#define SER_HOSTAUDIO "hostaudio"
#define SER_MULTICONT "multicontroller"
#define SER_AUDIOCFG "audiocfg"
#define SER_AUDIODEVICE "audiodevice"
#define SER_SESSIONVOLUMEDB "sessionvolumedb"
#define SER_VIDEOCFG "videocfg"
#define SER_HDR "hdr"
#define SER_YUV444 "yuv444"
#define SER_VIRTUALDISPLAY "virtualdisplay"
#define SER_VIDEODEC "videodec"
#define SER_WINDOWMODE "windowmode"
#define SER_MDNS "mdns"
#define SER_QUITAPPAFTER "quitAppAfter"
#define SER_ABSMOUSEMODE "mouseacceleration"
#define SER_ABSTOUCHMODE "abstouchmode"
#define SER_STARTWINDOWED "startwindowed"
#define SER_FRAMEPACING "framepacing"
#define SER_CONNWARNINGS "connwarnings"
#define SER_CONFWARNINGS "confwarnings"
#define SER_UIDISPLAYMODE "uidisplaymode"
#define SER_RICHPRESENCE "richpresence"
#define SER_GAMEPADMOUSE "gamepadmouse"
#define SER_DEFAULTVER "defaultver"
#define SER_PACKETSIZE "packetsize"
#define SER_DETECTNETBLOCKING "detectnetblocking"
#define SER_SHOWPERFOVERLAY "showperfoverlay"
#define SER_SWAPMOUSEBUTTONS "swapmousebuttons"
#define SER_MUTEONFOCUSLOSS "muteonfocusloss"
#define SER_BACKGROUNDGAMEPAD "backgroundgamepad"
#define SER_REVERSESCROLL "reversescroll"
#define SER_SWAPFACEBUTTONS "swapfacebuttons"
#define SER_CAPTURESYSKEYS "capturesyskeys"
#define SER_KEEPAWAKE "keepawake"
#define SER_LANGUAGE "language"
#define SER_RENDERER "renderer"

#define CURRENT_DEFAULT_VER 2

static StreamingPreferences* s_GlobalPrefs;

Q_GLOBAL_STATIC(QReadWriteLock, s_GlobalPrefsLock)

static const char* const kLegacyPreferenceKeys[] = {
    SER_WIDTH, SER_HEIGHT, SER_FPS, SER_BITRATE, SER_UNLOCK_BITRATE,
    SER_AUTOADJUSTBITRATE, SER_FULLSCREEN, SER_VSYNC, SER_GAMEOPTS,
    SER_HOSTAUDIO, SER_MULTICONT, SER_AUDIOCFG, SER_AUDIODEVICE,
    SER_SESSIONVOLUMEDB, SER_VIDEOCFG, SER_HDR,
    SER_YUV444, SER_VIRTUALDISPLAY, SER_VIDEODEC, SER_WINDOWMODE,
    SER_MDNS, SER_QUITAPPAFTER,
    SER_ABSMOUSEMODE, SER_ABSTOUCHMODE, SER_STARTWINDOWED, SER_FRAMEPACING,
    SER_CONNWARNINGS, SER_CONFWARNINGS, SER_UIDISPLAYMODE, SER_RICHPRESENCE,
    SER_GAMEPADMOUSE, SER_DEFAULTVER, SER_PACKETSIZE, SER_DETECTNETBLOCKING,
    SER_SHOWPERFOVERLAY, SER_SWAPMOUSEBUTTONS, SER_MUTEONFOCUSLOSS,
    SER_BACKGROUNDGAMEPAD, SER_REVERSESCROLL, SER_SWAPFACEBUTTONS,
    SER_CAPTURESYSKEYS, SER_KEEPAWAKE, SER_LANGUAGE, SER_RENDERER,
};

StreamingPreferences::StreamingPreferences(QQmlEngine *qmlEngine)
    : m_QmlEngine(qmlEngine)
{
    reload();
}

StreamingPreferences* StreamingPreferences::get(QQmlEngine *qmlEngine)
{
    {
        QReadLocker readGuard(s_GlobalPrefsLock);

        // If we have a preference object and it's associated with a QML engine or
        // if the caller didn't specify a QML engine, return the existing object.
        if (s_GlobalPrefs && (s_GlobalPrefs->m_QmlEngine || !qmlEngine)) {
            // The lifetime logic here relies on the QML engine also being a singleton.
            Q_ASSERT(!qmlEngine || s_GlobalPrefs->m_QmlEngine == qmlEngine);
            return s_GlobalPrefs;
        }
    }

    {
        QWriteLocker writeGuard(s_GlobalPrefsLock);

        // If we already have an preference object but the QML engine is now available,
        // associate the QML engine with the preferences.
        if (s_GlobalPrefs) {
            if (!s_GlobalPrefs->m_QmlEngine) {
                s_GlobalPrefs->m_QmlEngine = qmlEngine;
            }
            else {
                // We could reach this codepath if another thread raced with us
                // and created the object while we were outside the pref lock.
                Q_ASSERT(!qmlEngine || s_GlobalPrefs->m_QmlEngine == qmlEngine);
            }
        }
        else {
            s_GlobalPrefs = new StreamingPreferences(qmlEngine);
        }

        return s_GlobalPrefs;
    }
}

void StreamingPreferences::reload()
{
    SettingsDatabase* database = SettingsDatabase::get();
    QSettings legacySettings;

    if (database != nullptr && database->isOpen()) {
        QVariantMap legacyValues;
        for (const char* key : kLegacyPreferenceKeys) {
            const QString legacyKey = QString::fromLatin1(key);
            if (legacySettings.contains(legacyKey)) {
                legacyValues.insert(QStringLiteral("baseline.") + legacyKey,
                                    legacySettings.value(legacyKey));
            }
        }
        if (!database->importLegacySettings(
                    legacyValues,
                    QStringLiteral("migration.qsettings_baseline_imported_v2"))) {
            qWarning() << "Failed to import legacy StreamingPreferences:"
                       << database->lastError();
        }
    }

    auto value = [database, &legacySettings](const char* key,
                                             const QVariant& defaultValue) {
        const QString settingKey = QString::fromLatin1(key);
        if (database != nullptr && database->isOpen()) {
            return database->setting(QStringLiteral("baseline.") + settingKey,
                                     defaultValue);
        }
        return legacySettings.value(settingKey, defaultValue);
    };

    int defaultVer = value(SER_DEFAULTVER, 0).toInt();

#ifdef Q_OS_DARWIN
    recommendedFullScreenMode = WindowMode::WM_FULLSCREEN_DESKTOP;
#else
    // Wayland doesn't support modesetting, so use fullscreen desktop mode
    // unless we have a slow GPU (which can take advantage of wp_viewporter
    // to reduce GPU load with lower resolution video streams).
    if (WMUtils::isRunningWayland() && !WMUtils::isGpuSlow()) {
        recommendedFullScreenMode = WindowMode::WM_FULLSCREEN_DESKTOP;
    }
    else {
        recommendedFullScreenMode = WindowMode::WM_FULLSCREEN;
    }
#endif

    width = value(SER_WIDTH, 1280).toInt();
    height = value(SER_HEIGHT, 720).toInt();
    fps = value(SER_FPS, 60).toInt();
    enableYUV444 = value(SER_YUV444, false).toBool();
    useVirtualDisplay = value(SER_VIRTUALDISPLAY, false).toBool();
    bitrateKbps = value(SER_BITRATE, getDefaultBitrate(width, height, fps, enableYUV444)).toInt();
    unlockBitrate = value(SER_UNLOCK_BITRATE, false).toBool();
    autoAdjustBitrate = value(SER_AUTOADJUSTBITRATE, true).toBool();
    enableVsync = value(SER_VSYNC, true).toBool();
    gameOptimizations = value(SER_GAMEOPTS, true).toBool();
    playAudioOnHost = value(SER_HOSTAUDIO, false).toBool();
    multiController = value(SER_MULTICONT, true).toBool();
    enableMdns = value(SER_MDNS, true).toBool();
    quitAppAfter = value(SER_QUITAPPAFTER, false).toBool();
    absoluteMouseMode = value(SER_ABSMOUSEMODE, false).toBool();
    absoluteTouchMode = value(SER_ABSTOUCHMODE, true).toBool();
    framePacing = value(SER_FRAMEPACING, false).toBool();
    connectionWarnings = value(SER_CONNWARNINGS, true).toBool();
    configurationWarnings = value(SER_CONFWARNINGS, true).toBool();
    richPresence = value(SER_RICHPRESENCE, true).toBool();
    gamepadMouse = value(SER_GAMEPADMOUSE, true).toBool();
    detectNetworkBlocking = value(SER_DETECTNETBLOCKING, true).toBool();
    showPerformanceOverlay = value(SER_SHOWPERFOVERLAY, false).toBool();
    packetSize = value(SER_PACKETSIZE, 0).toInt();
    swapMouseButtons = value(SER_SWAPMOUSEBUTTONS, false).toBool();
    muteOnFocusLoss = value(SER_MUTEONFOCUSLOSS, false).toBool();
    backgroundGamepad = value(SER_BACKGROUNDGAMEPAD, false).toBool();
    reverseScrollDirection = value(SER_REVERSESCROLL, false).toBool();
    swapFaceButtons = value(SER_SWAPFACEBUTTONS, false).toBool();
    keepAwake = value(SER_KEEPAWAKE, true).toBool();
    enableHdr = value(SER_HDR, false).toBool();
    captureSysKeysMode = static_cast<CaptureSysKeysMode>(value(SER_CAPTURESYSKEYS,
                                                         static_cast<int>(CaptureSysKeysMode::CSK_OFF)).toInt());
    audioConfig = static_cast<AudioConfig>(value(SER_AUDIOCFG,
                                                  static_cast<int>(AudioConfig::AC_STEREO)).toInt());
    audioDevice = value(SER_AUDIODEVICE, QString()).toString();
    sessionVolumeDb = qBound(-60.0,
                             value(SER_SESSIONVOLUMEDB, 0.0).toDouble(),
                             0.0);
    videoCodecConfig = static_cast<VideoCodecConfig>(value(SER_VIDEOCFG,
                                                  static_cast<int>(VideoCodecConfig::VCC_AUTO)).toInt());
    videoDecoderSelection = static_cast<VideoDecoderSelection>(value(SER_VIDEODEC,
                                                  static_cast<int>(VideoDecoderSelection::VDS_AUTO)).toInt());
    rendererSelection = static_cast<RendererSelection>(value(SER_RENDERER,
                                                  static_cast<int>(RendererSelection::RS_AUTO)).toInt());
    windowMode = static_cast<WindowMode>(value(SER_WINDOWMODE,
                                                        // Try to load from the old preference value too
                                                        static_cast<int>(value(SER_FULLSCREEN, true).toBool() ?
                                                                             recommendedFullScreenMode : WindowMode::WM_WINDOWED)).toInt());
    uiDisplayMode = static_cast<UIDisplayMode>(value(SER_UIDISPLAYMODE,
                                               static_cast<int>(value(SER_STARTWINDOWED, true).toBool() ? UIDisplayMode::UI_WINDOWED
                                                                                                                 : UIDisplayMode::UI_MAXIMIZED)).toInt());
    language = static_cast<Language>(value(SER_LANGUAGE,
                                                    static_cast<int>(Language::LANG_AUTO)).toInt());


    // Perform default settings updates as required based on last default version
    if (defaultVer < 1) {
#ifdef Q_OS_DARWIN
        // Update window mode setting on macOS from full-screen (old default) to borderless windowed (new default)
        if (windowMode == WindowMode::WM_FULLSCREEN) {
            windowMode = WindowMode::WM_FULLSCREEN_DESKTOP;
        }
#endif
    }
    if (defaultVer < 2) {
        if (windowMode == WindowMode::WM_FULLSCREEN && WMUtils::isRunningWayland()) {
            windowMode = WindowMode::WM_FULLSCREEN_DESKTOP;
        }
    }

    // Fixup VCC value to the new settings format with codec and HDR separate
    if (videoCodecConfig == VCC_FORCE_HEVC_HDR_DEPRECATED) {
        videoCodecConfig = VCC_AUTO;
        enableHdr = true;
    }
}

bool StreamingPreferences::retranslate()
{
    static QTranslator* translator = nullptr;

#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
    if (m_QmlEngine != nullptr) {
        // Dynamic retranslation is not supported until Qt 5.10
        return false;
    }
#endif

    QTranslator* newTranslator = new QTranslator();
    QString languageSuffix = getSuffixFromLanguage(language);

    // Remove the old translator, even if we can't load a new one.
    // Otherwise we'll be stuck with the old translated values instead
    // of defaulting to English.
    if (translator != nullptr) {
        QCoreApplication::removeTranslator(translator);
        delete translator;
        translator = nullptr;
    }

    if (newTranslator->load(QString(":/languages/qml_") + languageSuffix)) {
        qInfo() << "Successfully loaded translation for" << languageSuffix;

        translator = newTranslator;
        QCoreApplication::installTranslator(translator);
    }
    else {
        qInfo() << "No translation available for" << languageSuffix;
        delete newTranslator;
    }

    if (m_QmlEngine != nullptr) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
        // This is a dynamic retranslation from the settings page.
        // We have to kick the QML engine into reloading our text.
        m_QmlEngine->retranslate();
#else
        // Unreachable below Qt 5.10 due to the check above
        Q_ASSERT(false);
#endif
    }
    else {
        // This is a translation from a non-QML context, which means
        // it is probably app startup. There's nothing to refresh.
    }

    return true;
}

QString StreamingPreferences::getSuffixFromLanguage(StreamingPreferences::Language lang)
{
    switch (lang)
    {
    case LANG_DE:
        return "de";
    case LANG_EN:
        return "en";
    case LANG_FR:
        return "fr";
    case LANG_ZH_CN:
        return "zh_CN";
    case LANG_NB_NO:
        return "nb_NO";
    case LANG_RU:
        return "ru";
    case LANG_ES:
        return "es";
    case LANG_JA:
        return "ja";
    case LANG_VI:
        return "vi";
    case LANG_TH:
        return "th";
    case LANG_KO:
        return "ko";
    case LANG_HU:
        return "hu";
    case LANG_NL:
        return "nl";
    case LANG_SV:
        return "sv";
    case LANG_TR:
        return "tr";
    case LANG_UK:
        return "uk";
    case LANG_ZH_TW:
        return "zh_TW";
    case LANG_PT:
        return "pt";
    case LANG_PT_BR:
        return "pt_BR";
    case LANG_EL:
        return "el";
    case LANG_IT:
        return "it";
    case LANG_HI:
        return "hi";
    case LANG_PL:
        return "pl";
    case LANG_CS:
        return "cs";
    case LANG_HE:
        return "he";
    case LANG_CKB:
        return "ckb";
    case LANG_LT:
        return "lt";
    case LANG_ET:
        return "et";
    case LANG_BG:
        return "bg";
    case LANG_EO:
        return "eo";
    case LANG_TA:
        return "ta";
    case LANG_AUTO:
    default:
        return QLocale::system().name();
    }
}

void StreamingPreferences::save()
{
    SettingsDatabase* database = SettingsDatabase::get();
    if (database == nullptr || !database->isOpen()) {
        qWarning() << "Settings database unavailable; baseline changes are session-only";
        return;
    }

    QVariantMap values;
    auto put = [&values](const char* key, const QVariant& value) {
        values.insert(QStringLiteral("baseline.") + QString::fromLatin1(key), value);
    };

    put(SER_WIDTH, width);
    put(SER_HEIGHT, height);
    put(SER_FPS, fps);
    put(SER_BITRATE, bitrateKbps);
    put(SER_UNLOCK_BITRATE, unlockBitrate);
    put(SER_AUTOADJUSTBITRATE, autoAdjustBitrate);
    put(SER_VSYNC, enableVsync);
    put(SER_GAMEOPTS, gameOptimizations);
    put(SER_HOSTAUDIO, playAudioOnHost);
    put(SER_MULTICONT, multiController);
    put(SER_MDNS, enableMdns);
    put(SER_QUITAPPAFTER, quitAppAfter);
    put(SER_ABSMOUSEMODE, absoluteMouseMode);
    put(SER_ABSTOUCHMODE, absoluteTouchMode);
    put(SER_FRAMEPACING, framePacing);
    put(SER_CONNWARNINGS, connectionWarnings);
    put(SER_CONFWARNINGS, configurationWarnings);
    put(SER_RICHPRESENCE, richPresence);
    put(SER_GAMEPADMOUSE, gamepadMouse);
    put(SER_PACKETSIZE, packetSize);
    put(SER_DETECTNETBLOCKING, detectNetworkBlocking);
    put(SER_SHOWPERFOVERLAY, showPerformanceOverlay);
    put(SER_AUDIOCFG, static_cast<int>(audioConfig));
    put(SER_AUDIODEVICE, audioDevice);
    put(SER_SESSIONVOLUMEDB, sessionVolumeDb);
    put(SER_HDR, enableHdr);
    put(SER_YUV444, enableYUV444);
    put(SER_VIRTUALDISPLAY, useVirtualDisplay);
    put(SER_VIDEOCFG, static_cast<int>(videoCodecConfig));
    put(SER_VIDEODEC, static_cast<int>(videoDecoderSelection));
    put(SER_RENDERER, static_cast<int>(rendererSelection));
    put(SER_WINDOWMODE, static_cast<int>(windowMode));
    put(SER_UIDISPLAYMODE, static_cast<int>(uiDisplayMode));
    put(SER_LANGUAGE, static_cast<int>(language));
    put(SER_DEFAULTVER, CURRENT_DEFAULT_VER);
    put(SER_SWAPMOUSEBUTTONS, swapMouseButtons);
    put(SER_MUTEONFOCUSLOSS, muteOnFocusLoss);
    put(SER_BACKGROUNDGAMEPAD, backgroundGamepad);
    put(SER_REVERSESCROLL, reverseScrollDirection);
    put(SER_SWAPFACEBUTTONS, swapFaceButtons);
    put(SER_CAPTURESYSKEYS, static_cast<int>(captureSysKeysMode));
    put(SER_KEEPAWAKE, keepAwake);

    if (!database->setSettings(values)) {
        qWarning() << "Failed to save StreamingPreferences:"
                   << database->lastError();
    }
}

QVariantMap StreamingPreferences::toVariantMap() const
{
    QVariantMap values;
    values.insert(SER_WIDTH, width);
    values.insert(SER_HEIGHT, height);
    values.insert(SER_FPS, fps);
    values.insert(SER_BITRATE, bitrateKbps);
    values.insert(SER_UNLOCK_BITRATE, unlockBitrate);
    values.insert(SER_AUTOADJUSTBITRATE, autoAdjustBitrate);
    values.insert(SER_VSYNC, enableVsync);
    values.insert(SER_GAMEOPTS, gameOptimizations);
    values.insert(SER_HOSTAUDIO, playAudioOnHost);
    values.insert(SER_MULTICONT, multiController);
    values.insert(SER_MDNS, enableMdns);
    values.insert(SER_QUITAPPAFTER, quitAppAfter);
    values.insert(SER_ABSMOUSEMODE, absoluteMouseMode);
    values.insert(SER_ABSTOUCHMODE, absoluteTouchMode);
    values.insert(SER_FRAMEPACING, framePacing);
    values.insert(SER_CONNWARNINGS, connectionWarnings);
    values.insert(SER_CONFWARNINGS, configurationWarnings);
    values.insert(SER_RICHPRESENCE, richPresence);
    values.insert(SER_GAMEPADMOUSE, gamepadMouse);
    values.insert(SER_PACKETSIZE, packetSize);
    values.insert(SER_DETECTNETBLOCKING, detectNetworkBlocking);
    values.insert(SER_SHOWPERFOVERLAY, showPerformanceOverlay);
    values.insert(SER_AUDIOCFG, static_cast<int>(audioConfig));
    values.insert(SER_AUDIODEVICE, audioDevice);
    values.insert(SER_SESSIONVOLUMEDB, sessionVolumeDb);
    values.insert(SER_HDR, enableHdr);
    values.insert(SER_YUV444, enableYUV444);
    values.insert(SER_VIRTUALDISPLAY, useVirtualDisplay);
    values.insert(SER_VIDEOCFG, static_cast<int>(videoCodecConfig));
    values.insert(SER_VIDEODEC, static_cast<int>(videoDecoderSelection));
    values.insert(SER_RENDERER, static_cast<int>(rendererSelection));
    values.insert(SER_WINDOWMODE, static_cast<int>(windowMode));
    values.insert(SER_UIDISPLAYMODE, static_cast<int>(uiDisplayMode));
    values.insert(SER_LANGUAGE, static_cast<int>(language));
    values.insert(SER_SWAPMOUSEBUTTONS, swapMouseButtons);
    values.insert(SER_MUTEONFOCUSLOSS, muteOnFocusLoss);
    values.insert(SER_BACKGROUNDGAMEPAD, backgroundGamepad);
    values.insert(SER_REVERSESCROLL, reverseScrollDirection);
    values.insert(SER_SWAPFACEBUTTONS, swapFaceButtons);
    values.insert(SER_CAPTURESYSKEYS, static_cast<int>(captureSysKeysMode));
    values.insert(SER_KEEPAWAKE, keepAwake);
    return values;
}

void StreamingPreferences::applyVariantMap(const QVariantMap& values)
{
    auto assignInt = [&values](const char* key, int& target) {
        if (values.contains(key)) target = values.value(key).toInt();
    };
    auto assignBool = [&values](const char* key, bool& target) {
        if (values.contains(key)) target = values.value(key).toBool();
    };

    assignInt(SER_WIDTH, width);
    assignInt(SER_HEIGHT, height);
    assignInt(SER_FPS, fps);
    assignInt(SER_BITRATE, bitrateKbps);
    assignBool(SER_UNLOCK_BITRATE, unlockBitrate);
    assignBool(SER_AUTOADJUSTBITRATE, autoAdjustBitrate);
    assignBool(SER_VSYNC, enableVsync);
    assignBool(SER_GAMEOPTS, gameOptimizations);
    assignBool(SER_HOSTAUDIO, playAudioOnHost);
    assignBool(SER_MULTICONT, multiController);
    assignBool(SER_MDNS, enableMdns);
    assignBool(SER_QUITAPPAFTER, quitAppAfter);
    assignBool(SER_ABSMOUSEMODE, absoluteMouseMode);
    assignBool(SER_ABSTOUCHMODE, absoluteTouchMode);
    assignBool(SER_FRAMEPACING, framePacing);
    assignBool(SER_CONNWARNINGS, connectionWarnings);
    assignBool(SER_CONFWARNINGS, configurationWarnings);
    assignBool(SER_RICHPRESENCE, richPresence);
    assignBool(SER_GAMEPADMOUSE, gamepadMouse);
    assignInt(SER_PACKETSIZE, packetSize);
    assignBool(SER_DETECTNETBLOCKING, detectNetworkBlocking);
    assignBool(SER_SHOWPERFOVERLAY, showPerformanceOverlay);
    assignBool(SER_HDR, enableHdr);
    assignBool(SER_YUV444, enableYUV444);
    assignBool(SER_VIRTUALDISPLAY, useVirtualDisplay);
    assignBool(SER_SWAPMOUSEBUTTONS, swapMouseButtons);
    assignBool(SER_MUTEONFOCUSLOSS, muteOnFocusLoss);
    assignBool(SER_BACKGROUNDGAMEPAD, backgroundGamepad);
    assignBool(SER_REVERSESCROLL, reverseScrollDirection);
    assignBool(SER_SWAPFACEBUTTONS, swapFaceButtons);
    assignBool(SER_KEEPAWAKE, keepAwake);

    if (values.contains(SER_AUDIOCFG))
        audioConfig = static_cast<AudioConfig>(values.value(SER_AUDIOCFG).toInt());
    if (values.contains(SER_AUDIODEVICE))
        audioDevice = values.value(SER_AUDIODEVICE).toString();
    if (values.contains(SER_SESSIONVOLUMEDB))
        sessionVolumeDb = qBound(-60.0,
                                 values.value(SER_SESSIONVOLUMEDB).toDouble(),
                                 0.0);
    if (values.contains(SER_VIDEOCFG))
        videoCodecConfig = static_cast<VideoCodecConfig>(values.value(SER_VIDEOCFG).toInt());
    if (values.contains(SER_VIDEODEC))
        videoDecoderSelection = static_cast<VideoDecoderSelection>(values.value(SER_VIDEODEC).toInt());
    if (values.contains(SER_RENDERER))
        rendererSelection = static_cast<RendererSelection>(values.value(SER_RENDERER).toInt());
    if (values.contains(SER_WINDOWMODE))
        windowMode = static_cast<WindowMode>(values.value(SER_WINDOWMODE).toInt());
    if (values.contains(SER_UIDISPLAYMODE))
        uiDisplayMode = static_cast<UIDisplayMode>(values.value(SER_UIDISPLAYMODE).toInt());
    if (values.contains(SER_LANGUAGE))
        language = static_cast<Language>(values.value(SER_LANGUAGE).toInt());
    if (values.contains(SER_CAPTURESYSKEYS))
        captureSysKeysMode = static_cast<CaptureSysKeysMode>(
                    values.value(SER_CAPTURESYSKEYS).toInt());
}

StreamingPreferences* StreamingPreferences::clone() const
{
    StreamingPreferences* copy = new StreamingPreferences(nullptr);
    copy->applyVariantMap(toVariantMap());
    return copy;
}

int StreamingPreferences::getDefaultBitrate(int width, int height, int fps, bool yuv444)
{
    // Don't scale bitrate linearly beyond 60 FPS. It's definitely not a linear
    // bitrate increase for frame rate once we get to values that high.
    float frameRateFactor = (fps <= 60 ? fps : (qSqrt(fps / 60.f) * 60.f)) / 30.f;

    // TODO: Collect some empirical data to see if these defaults make sense.
    // We're just using the values that the Shield used, as we have for years.
    static const struct resTable {
        int pixels;
        int factor;
    } resTable[] {
        { 640 * 360, 1 },
        { 854 * 480, 2 },
        { 1280 * 720, 5 },
        { 1920 * 1080, 10 },
        { 2560 * 1440, 20 },
        { 3840 * 2160, 40 },
        { -1, -1 },
    };

    // Calculate the resolution factor by linear interpolation of the resolution table
    float resolutionFactor;
    int pixels = width * height;
    for (int i = 0;; i++) {
        if (pixels == resTable[i].pixels) {
            // We can bail immediately for exact matches
            resolutionFactor = resTable[i].factor;
            break;
        }
        else if (pixels < resTable[i].pixels) {
            if (i == 0) {
                // Never go below the lowest resolution entry
                resolutionFactor = resTable[i].factor;
            }
            else {
                // Interpolate between the entry greater than the chosen resolution (i) and the entry less than the chosen resolution (i-1)
                resolutionFactor = ((float)(pixels - resTable[i-1].pixels) / (resTable[i].pixels - resTable[i-1].pixels)) * (resTable[i].factor - resTable[i-1].factor) + resTable[i-1].factor;
            }
            break;
        }
        else if (resTable[i].pixels == -1) {
            // Never go above the highest resolution entry
            resolutionFactor = resTable[i-1].factor;
            break;
        }
    }

    if (yuv444) {
        // This is rough estimation based on the fact that 4:4:4 doubles the amount of raw YUV data compared to 4:2:0
        resolutionFactor *= 2;
    }

    return qRound(resolutionFactor * frameRateFactor) * 1000;
}
