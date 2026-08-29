#include "systemproperties.h"
#include "utils.h"
#include "core/settingsdatabase.h"
#include "settings/effectivesettingsresolver.h"

#include <QGuiApplication>
#include <QLibraryInfo>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QScreen>
#include <QTimer>

#include "streaming/session.h"
#include "streaming/streamutils.h"

#ifdef Q_OS_WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <dxgi1_6.h>
#endif

#ifdef Q_OS_DARWIN
#include <ApplicationServices/ApplicationServices.h>
#include <objc/message.h>
#include <objc/runtime.h>
#endif

namespace {
struct NativeDisplayInfo
{
    QString fingerprint;
    bool hdrCapable = false;
};

QString stableHash(const QString& value)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(value.toUtf8(),
                                 QCryptographicHash::Sha256).toHex());
}

#ifdef Q_OS_DARWIN
bool macDisplaySupportsHdr(CGDirectDisplayID displayId,
                           int screenIndex)
{
    using SendId = id (*)(id, SEL);
    using SendIdArg = id (*)(id, SEL, id);
    using SendIdCString = id (*)(id, SEL, const char*);
    using SendCount = unsigned long (*)(id, SEL);
    using SendAt = id (*)(id, SEL, unsigned long);
    using SendUInt = unsigned int (*)(id, SEL);
    using SendDouble = double (*)(id, SEL);

    id screenClass = reinterpret_cast<id>(objc_getClass("NSScreen"));
    if (screenClass == nullptr) return false;
    const SEL screensSelector = sel_registerName("screens");
    id screens = reinterpret_cast<SendId>(objc_msgSend)(
        screenClass, screensSelector);
    if (screens == nullptr) return false;
    id stringClass = reinterpret_cast<id>(objc_getClass("NSString"));
    id screenNumberKey =
        reinterpret_cast<SendIdCString>(objc_msgSend)(
            stringClass,
            sel_registerName("stringWithUTF8String:"),
            "NSScreenNumber");
    const unsigned long count =
        reinterpret_cast<SendCount>(objc_msgSend)(
            screens, sel_registerName("count"));
    const SEL edrSelector = sel_registerName(
        "maximumPotentialExtendedDynamicRangeColorComponentValue");
    for (unsigned long i = 0; i < count; ++i) {
        id screen = reinterpret_cast<SendAt>(objc_msgSend)(
            screens, sel_registerName("objectAtIndex:"), i);
        id description = reinterpret_cast<SendId>(objc_msgSend)(
            screen, sel_registerName("deviceDescription"));
        id number = reinterpret_cast<SendIdArg>(objc_msgSend)(
            description, sel_registerName("objectForKey:"),
            screenNumberKey);
        const unsigned int screenDisplayId =
            reinterpret_cast<SendUInt>(objc_msgSend)(
                number, sel_registerName("unsignedIntValue"));
        if ((displayId != 0 && screenDisplayId != displayId)
                || (displayId == 0
                    && static_cast<int>(i) != screenIndex)) {
            continue;
        }
        return reinterpret_cast<SendDouble>(objc_msgSend)(
                   screen, edrSelector) > 1.0;
    }
    return false;
}
#endif

#ifdef Q_OS_LINUX
bool edidSupportsHdr(const QByteArray& edid)
{
    if (edid.size() < 256) return false;
    const auto byte = [&edid](int offset) {
        return static_cast<quint8>(edid.at(offset));
    };
    const int extensionCount = byte(126);
    for (int extension = 0; extension < extensionCount; ++extension) {
        const int base = 128 * (extension + 1);
        if (base + 127 >= edid.size() || byte(base) != 0x02) continue;
        const int dataEnd = byte(base + 2) == 0
            ? base + 127 : base + byte(base + 2);
        for (int offset = base + 4;
             offset < dataEnd && offset < base + 127;) {
            const quint8 header = byte(offset);
            const int tag = header >> 5;
            const int length = header & 0x1f;
            if (tag == 7 && length > 0
                    && byte(offset + 1) == 0x06) {
                return true;
            }
            offset += length + 1;
        }
    }
    return false;
}
#endif

NativeDisplayInfo nativeDisplayInfo(int displayIndex,
                                    const QString& name,
                                    const QRect& bounds)
{
    NativeDisplayInfo info;
    QString identity = name;
    if (QScreen* screen =
            QGuiApplication::screens().value(displayIndex, nullptr)) {
        identity += QLatin1Char('|') + screen->manufacturer()
            + QLatin1Char('|') + screen->model()
            + QLatin1Char('|') + screen->serialNumber();
    }

#ifdef Q_OS_DARWIN
    CGDirectDisplayID displays[32] = {};
    uint32_t displayCount = 0;
    CGDirectDisplayID matched = 0;
    if (CGGetActiveDisplayList(32, displays, &displayCount)
            == kCGErrorSuccess) {
        for (uint32_t i = 0; i < displayCount; ++i) {
            const CGRect nativeBounds = CGDisplayBounds(displays[i]);
            if (qRound(nativeBounds.origin.x) == bounds.x()
                    && qRound(nativeBounds.origin.y) == bounds.y()
                    && qRound(nativeBounds.size.width) == bounds.width()
                    && qRound(nativeBounds.size.height) == bounds.height()) {
                matched = displays[i];
                break;
            }
        }
        if (matched == 0
                && displayIndex >= 0
                && static_cast<uint32_t>(displayIndex) < displayCount) {
            matched = displays[displayIndex];
        }
        if (matched != 0) {
            identity = QStringLiteral("mac:%1:%2:%3|%4")
                .arg(CGDisplayVendorNumber(matched))
                .arg(CGDisplayModelNumber(matched))
                .arg(CGDisplaySerialNumber(matched))
                .arg(identity);
        }
    }
    info.hdrCapable = macDisplaySupportsHdr(matched, displayIndex);
#elif defined(Q_OS_WIN32)
    IDXGIFactory1* factory = nullptr;
    if (SUCCEEDED(CreateDXGIFactory1(
            __uuidof(IDXGIFactory1),
            reinterpret_cast<void**>(&factory)))) {
        IDXGIAdapter1* adapter = nullptr;
        for (UINT adapterIndex = 0;
             factory->EnumAdapters1(adapterIndex, &adapter)
                 != DXGI_ERROR_NOT_FOUND;
             ++adapterIndex) {
            IDXGIOutput* output = nullptr;
            for (UINT outputIndex = 0;
                 adapter->EnumOutputs(outputIndex, &output)
                     != DXGI_ERROR_NOT_FOUND;
                 ++outputIndex) {
                DXGI_OUTPUT_DESC description = {};
                output->GetDesc(&description);
                const RECT& rect = description.DesktopCoordinates;
                if (rect.left == bounds.left()
                        && rect.top == bounds.top()
                        && rect.right - rect.left == bounds.width()
                        && rect.bottom - rect.top == bounds.height()) {
                    identity = QStringLiteral("win:")
                        + QString::fromWCharArray(description.DeviceName);
                    IDXGIOutput6* output6 = nullptr;
                    if (SUCCEEDED(output->QueryInterface(
                            __uuidof(IDXGIOutput6),
                            reinterpret_cast<void**>(&output6)))) {
                        DXGI_OUTPUT_DESC1 description1 = {};
                        if (SUCCEEDED(output6->GetDesc1(&description1))) {
                            info.hdrCapable =
                                description1.ColorSpace
                                    == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020
                                && description1.BitsPerColor >= 10;
                        }
                        output6->Release();
                    }
                }
                output->Release();
                output = nullptr;
            }
            adapter->Release();
            adapter = nullptr;
        }
        factory->Release();
    }
#elif defined(Q_OS_LINUX)
    const QString screenName =
        QGuiApplication::screens().value(displayIndex)
            ? QGuiApplication::screens().value(displayIndex)->name()
            : name;
    QDir drm(QStringLiteral("/sys/class/drm"));
    const QStringList connectors = drm.entryList(
        QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& connector : connectors) {
        if (!connector.endsWith(QLatin1Char('-') + screenName,
                                Qt::CaseInsensitive)) {
            continue;
        }
        QFile status(drm.filePath(connector + QStringLiteral("/status")));
        if (!status.open(QIODevice::ReadOnly)
                || status.readAll().trimmed() != "connected") {
            continue;
        }
        QFile edidFile(drm.filePath(connector + QStringLiteral("/edid")));
        if (!edidFile.open(QIODevice::ReadOnly)) continue;
        const QByteArray edid = edidFile.readAll();
        if (!edid.isEmpty()) {
            identity = QStringLiteral("linux-edid:")
                + QString::fromLatin1(
                    QCryptographicHash::hash(
                        edid, QCryptographicHash::Sha256).toHex());
            info.hdrCapable = edidSupportsHdr(edid);
        }
        break;
    }
#endif

    info.fingerprint = stableHash(identity);
    return info;
}
}

class SystemPropertyQueryThread : public QThread
{
public:
    SystemPropertyQueryThread(SystemProperties* properties)
        : QThread(properties), m_Properties(properties)
    {
        setObjectName("System Properties Async Query Thread");
    }

private:
    void run() override
    {
        bool hasHardwareAcceleration;
        bool rendererAlwaysFullScreen;
        bool supportsHdr;
        QSize maximumResolution;

        Session::getDecoderInfo(m_Properties->testWindow, hasHardwareAcceleration, rendererAlwaysFullScreen, supportsHdr, maximumResolution);

        // Propagate the decoder properties to the SystemProperties singleton and emit any change signals on the main thread
        QMetaObject::invokeMethod(m_Properties, "updateDecoderProperties",
                                  Qt::QueuedConnection,
                                  Q_ARG(bool, hasHardwareAcceleration),
                                  Q_ARG(bool, rendererAlwaysFullScreen),
                                  Q_ARG(QSize, maximumResolution),
                                  Q_ARG(bool, supportsHdr));
    }

private:
    SystemProperties* m_Properties;
};

SystemProperties::SystemProperties()
{
    // Jochona: warm the SDL game-controller subsystem immediately, on a worker
    // thread. Controller init runs HID enumeration inside a nested CFRunLoop;
    // on macOS that can be asked to flush a CoreAnimation transaction
    // mid-enumeration, which wedges the window's first frame (pinwheel at
    // launch) when it happens on the UI thread during window creation. QML
    // waits for gamepadProbeComplete before enabling controller navigation,
    // and SdlGamepadKeyNavigation::enable() then just takes the refcount
    // shortcut because the subsystem is already up.
    QThread* gamepadProbeThread = QThread::create([this]() {
        QString unmapped = SdlInputHandler::getUnmappedGamepads();
        QMetaObject::invokeMethod(this, [this, unmapped]() {
            unmappedGamepads = unmapped;
            gamepadProbeComplete = true;
            emit gamepadProbeCompleteChanged();
            if (!unmapped.isEmpty()) {
                emit unmappedGamepadsChanged();
            }
        }, Qt::QueuedConnection);
    });
    gamepadProbeThread->setObjectName("Gamepad Probe Thread");
    connect(gamepadProbeThread, &QThread::finished, gamepadProbeThread, &QThread::deleteLater);
    gamepadProbeThread->start();
    hasDesktopEnvironment = WMUtils::isRunningDesktopEnvironment();
    isRunningWayland = WMUtils::isRunningWayland();
    isRunningXWayland = isRunningWayland && QGuiApplication::platformName() == "xcb";
    usesMaterial3Theme = QLibraryInfo::version() >= QVersionNumber(6, 5, 0);

#ifdef Q_OS_DARWIN
    isDarwin = true;
#else
    isDarwin = false;
#endif

    QString nativeArch = QSysInfo::currentCpuArchitecture();

#ifdef Q_OS_WIN32
    {
        USHORT processArch, machineArch;

        // Use IsWow64Process2() because it doesn't lie on ARM64
        if (IsWow64Process2(GetCurrentProcess(), &processArch, &machineArch)) {
            switch (machineArch) {
            case IMAGE_FILE_MACHINE_I386:
                nativeArch = "i386";
                break;
            case IMAGE_FILE_MACHINE_AMD64:
                nativeArch = "x86_64";
                break;
            case IMAGE_FILE_MACHINE_ARM64:
                nativeArch = "arm64";
                break;
            }
        }

        isWow64 = nativeArch != QSysInfo::buildCpuArchitecture();
    }
#else
    isWow64 = false;
#endif

    if (nativeArch == "i386") {
        friendlyNativeArchName = "x86";
    }
    else if (nativeArch == "x86_64") {
        friendlyNativeArchName = "x64";
    }
    else {
        friendlyNativeArchName = nativeArch.toUpper();
    }

    // Assume we can probably launch a browser if we're in a GUI environment
    hasBrowser = hasDesktopEnvironment;

#ifdef HAVE_DISCORD
    hasDiscordIntegration = true;
#else
    hasDiscordIntegration = false;
#endif

    // These will be queried asynchronously to avoid blocking the UI
    hasHardwareAcceleration = true;
    rendererAlwaysFullScreen = false;
    supportsHdr = true;
    maximumResolution = QSize(0, 0);

    auto refreshAfterDisplayChange = [this](QScreen*) {
        QTimer::singleShot(250, this, &SystemProperties::refreshDisplays);
    };
    connect(qGuiApp, &QGuiApplication::screenAdded,
            this, refreshAfterDisplayChange);
    connect(qGuiApp, &QGuiApplication::screenRemoved,
            this, refreshAfterDisplayChange);
    connect(qGuiApp, &QGuiApplication::primaryScreenChanged,
            this, refreshAfterDisplayChange);
}

SystemProperties::~SystemProperties()
{
    waitForAsyncLoad();
}

void SystemProperties::updateDecoderProperties(bool hasHardwareAcceleration, bool rendererAlwaysFullScreen, QSize maximumResolution, bool supportsHdr)
{
    SDL_assert(testWindow);

    if (hasHardwareAcceleration != this->hasHardwareAcceleration) {
        this->hasHardwareAcceleration = hasHardwareAcceleration;
        emit hasHardwareAccelerationChanged();
    }

    if (rendererAlwaysFullScreen != this->rendererAlwaysFullScreen) {
        this->rendererAlwaysFullScreen = rendererAlwaysFullScreen;
        emit rendererAlwaysFullScreenChanged();
    }

    if (maximumResolution != this->maximumResolution) {
        this->maximumResolution = maximumResolution;
        emit maximumResolutionChanged();
    }

    if (supportsHdr != this->supportsHdr) {
        this->supportsHdr = supportsHdr;
        emit supportsHdrChanged();
    }

    SDL_DestroyWindow(testWindow);
    testWindow = nullptr;
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

QRect SystemProperties::getNativeResolution(int displayIndex)
{
    // Returns default constructed QRect if out of bounds
    return monitorNativeResolutions.value(displayIndex);
}

QRect SystemProperties::getSafeAreaResolution(int displayIndex)
{
    // Returns default constructed QRect if out of bounds
    return monitorSafeAreaResolutions.value(displayIndex);
}

int SystemProperties::getRefreshRate(int displayIndex)
{
    // Returns 0 if out of bounds
    return monitorRefreshRates.value(displayIndex);
}

QVariantMap SystemProperties::getDisplayContext(int displayIndex) const
{
    return displayContexts.value(displayIndex).toMap();
}

void SystemProperties::refreshAudioOutputs()
{
    const bool alreadyInitialized = SDL_WasInit(SDL_INIT_AUDIO);
    if (!alreadyInitialized && SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Unable to enumerate audio outputs: %s",
                    SDL_GetError());
        return;
    }
    QStringList outputs;
    const int count = SDL_GetNumAudioDevices(0);
    for (int index = 0; index < count; ++index) {
        const char* name = SDL_GetAudioDeviceName(index, 0);
        if (name != nullptr && name[0] != '\0') {
            outputs.append(QString::fromUtf8(name));
        }
    }
    outputs.removeDuplicates();
    if (!alreadyInitialized) SDL_QuitSubSystem(SDL_INIT_AUDIO);
    if (audioOutputDevices != outputs) {
        audioOutputDevices = outputs;
        emit audioOutputDevicesChanged();
    }
}

void SystemProperties::startAsyncLoad()
{
    if (systemPropertyQueryThread) {
        // Already started/completed
        return;
    }
    refreshAudioOutputs();

    // We initialize the video subsystem and test window on the main thread
    // because some platforms (macOS) do not support window creation on
    // non-main threads.
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_InitSubSystem(SDL_INIT_VIDEO) failed: %s",
                     SDL_GetError());
        return;
    }

    testWindow = StreamUtils::createTestWindow();
    if (!testWindow) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to create window for hardware decode test: %s",
                     SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }

    // Update display related attributes (max FPS, native resolution, etc).
    //
    // NB: SDL3 will forcefully refresh displays when a window is created,
    // so we place this after the window creation to ensure we don't pay
    // the penalty for mode enumeration twice.
    refreshDisplays();

    systemPropertyQueryThread = new SystemPropertyQueryThread(this);
    systemPropertyQueryThread->start();
}

void SystemProperties::waitForAsyncLoad()
{
    if (systemPropertyQueryThread) {
        systemPropertyQueryThread->wait();
    }
}

void SystemProperties::refreshDisplays()
{
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_InitSubSystem(SDL_INIT_VIDEO) failed: %s",
                     SDL_GetError());
        return;
    }

    struct PendingDisplay
    {
        int sdlIndex;
        QString name;
        QRect bounds;
        QRect nativeResolution;
        QRect safeResolution;
        int refreshRate;
        NativeDisplayInfo native;
    };
    QList<PendingDisplay> pending;
    monitorNativeResolutions.clear();
    monitorSafeAreaResolutions.clear();
    monitorRefreshRates.clear();

    const QList<QScreen*> qtScreens = QGuiApplication::screens();
    for (int displayIndex = 0;
         displayIndex < qtScreens.size(); ++displayIndex) {
        QScreen* qtScreen = qtScreens.at(displayIndex);
        SDL_DisplayMode desktopMode = {};
        SDL_Rect safeArea = {};
        StreamUtils::getNativeDesktopMode(
            displayIndex, &desktopMode, &safeArea);
        if (desktopMode.w <= 0 || desktopMode.h <= 0) {
            const qreal scale = qtScreen->devicePixelRatio();
            desktopMode.w = qRound(qtScreen->geometry().width() * scale);
            desktopMode.h = qRound(qtScreen->geometry().height() * scale);
            desktopMode.refresh_rate =
                qRound(qtScreen->refreshRate());
            safeArea = {0, 0, desktopMode.w, desktopMode.h};
        }
        if (desktopMode.w > 8192 || desktopMode.h > 8192) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Skipping resolution over 8K: %dx%d",
                        desktopMode.w, desktopMode.h);
            continue;
        }

        SDL_DisplayMode bestMode = desktopMode;
        const int modeCount = SDL_GetNumDisplayModes(displayIndex);
        for (int modeIndex = 0; modeIndex < modeCount; ++modeIndex) {
            SDL_DisplayMode mode = {};
            if (SDL_GetDisplayMode(displayIndex, modeIndex, &mode) == 0
                    && mode.w == desktopMode.w
                    && mode.h == desktopMode.h
                    && mode.refresh_rate > bestMode.refresh_rate) {
                bestMode = mode;
            }
        }
        int refreshRate = bestMode.refresh_rate;
        if (refreshRate >= 58 && refreshRate <= 62) refreshRate = 60;
        else if (refreshRate >= 28 && refreshRate <= 32) refreshRate = 30;
        if (refreshRate <= 0) {
            refreshRate = qRound(qtScreen->refreshRate());
        }

        SDL_Rect displayBounds = {};
        QRect bounds = qtScreen->geometry();
        if (SDL_GetDisplayBounds(displayIndex, &displayBounds) == 0
                && displayBounds.w > 0 && displayBounds.h > 0) {
            bounds = QRect(displayBounds.x, displayBounds.y,
                           displayBounds.w, displayBounds.h);
        }
        const QString name = qtScreen->name().isEmpty()
            ? tr("Display %1").arg(displayIndex + 1)
            : qtScreen->name();
        PendingDisplay display{
            displayIndex,
            name,
            bounds,
            QRect(0, 0, desktopMode.w, desktopMode.h),
            QRect(0, 0, safeArea.w, safeArea.h),
            refreshRate,
            nativeDisplayInfo(displayIndex, name, bounds),
        };
        pending.append(display);
        monitorNativeResolutions.append(display.nativeResolution);
        monitorSafeAreaResolutions.append(display.safeResolution);
        monitorRefreshRates.append(refreshRate);
    }
    SDL_QuitSubSystem(SDL_INIT_VIDEO);

    QStringList fingerprints;
    for (const PendingDisplay& display : std::as_const(pending)) {
        fingerprints.append(display.native.fingerprint);
    }
    fingerprints.sort();
    const QString dockState =
        QStringLiteral("%1:%2")
            .arg(pending.size() > 1
                     ? QStringLiteral("docked")
                     : QStringLiteral("single"),
                 stableHash(fingerprints.join(QLatin1Char('|'))));

    SettingsDatabase* database = SettingsDatabase::get();
    const QString deviceId =
        EffectiveSettingsResolver::get()->clientDeviceId();
    QVariantList nextContexts;
    QScreen* activeScreen = QGuiApplication::focusWindow()
        ? QGuiApplication::focusWindow()->screen()
        : QGuiApplication::primaryScreen();
    const int activeQtIndex =
        QGuiApplication::screens().indexOf(activeScreen);
    QString nextActiveContextId;
    bool nextActiveHdr = false;

    for (const PendingDisplay& display : std::as_const(pending)) {
        const QString contextId = stableHash(
            deviceId + QLatin1Char('|')
            + display.native.fingerprint + QLatin1Char('|') + dockState);
        const QVariantMap metadata{
            {QStringLiteral("width"), display.nativeResolution.width()},
            {QStringLiteral("height"), display.nativeResolution.height()},
            {QStringLiteral("refreshHz"), display.refreshRate},
            {QStringLiteral("hdrCapable"), display.native.hdrCapable},
        };
        if (database != nullptr && database->isOpen()) {
            if (!database->upsertDisplayContext(
                    contextId, deviceId, display.name,
                    display.native.fingerprint, dockState, metadata)) {
                qWarning() << "Failed to persist Display Context"
                           << contextId << database->lastError();
            }
        }
        QVariantMap context = metadata;
        context.insert(QStringLiteral("id"), contextId);
        context.insert(QStringLiteral("name"), display.name);
        context.insert(QStringLiteral("fingerprint"),
                       display.native.fingerprint);
        context.insert(QStringLiteral("dockState"), dockState);
        context.insert(QStringLiteral("bounds"), display.bounds);
        nextContexts.append(context);

        const bool isActive =
            display.sdlIndex == activeQtIndex
            || (activeScreen != nullptr
                && display.name == activeScreen->name());
        if (isActive || nextActiveContextId.isEmpty()) {
            nextActiveContextId = contextId;
            nextActiveHdr = display.native.hdrCapable;
        }
    }

    const QString previousContextId = activeDisplayContextId;
    if (displayContexts != nextContexts) {
        displayContexts = nextContexts;
        emit displayContextsChanged();
    }
    if (activeDisplayContextId != nextActiveContextId) {
        activeDisplayContextId = nextActiveContextId;
        emit activeDisplayContextIdChanged();
    }
    if (activeDisplaySupportsHdr != nextActiveHdr) {
        activeDisplaySupportsHdr = nextActiveHdr;
        emit activeDisplaySupportsHdrChanged();
    }
    if (database != nullptr && database->isOpen()) {
        database->setSetting(QStringLiteral("display.active_context_id"),
                             activeDisplayContextId);
    }
    if (!previousContextId.isEmpty()
            && previousContextId != activeDisplayContextId) {
        emit displayTopologyChanged(previousContextId,
                                    activeDisplayContextId);
    }
}
