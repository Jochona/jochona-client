//
// SPDX-FileCopyrightText: Lunaframe Client Contributors
//
// SPDX-License-Identifier: GPL-3.0-only
//
#include "controllermanager.h"

#include <QtGlobal>

#define POLL_INTERVAL_MS 33 // ~30Hz

namespace {

// Digital buttons in the fixed logical vocabulary shared with Glyphs.qml
// (lt/rt are synthesized from analog trigger axes below, not listed here).
struct DigitalButtonMapping
{
    const char* logicalName;
    SDL_GameControllerButton sdlButton;
};

const DigitalButtonMapping k_DigitalButtons[] = {
    { "a", SDL_CONTROLLER_BUTTON_A },
    { "b", SDL_CONTROLLER_BUTTON_B },
    { "x", SDL_CONTROLLER_BUTTON_X },
    { "y", SDL_CONTROLLER_BUTTON_Y },
    { "lb", SDL_CONTROLLER_BUTTON_LEFTSHOULDER },
    { "rb", SDL_CONTROLLER_BUTTON_RIGHTSHOULDER },
    { "dpad_up", SDL_CONTROLLER_BUTTON_DPAD_UP },
    { "dpad_down", SDL_CONTROLLER_BUTTON_DPAD_DOWN },
    { "dpad_left", SDL_CONTROLLER_BUTTON_DPAD_LEFT },
    { "dpad_right", SDL_CONTROLLER_BUTTON_DPAD_RIGHT },
    { "stick_left", SDL_CONTROLLER_BUTTON_LEFTSTICK },
    { "stick_right", SDL_CONTROLLER_BUTTON_RIGHTSTICK },
    { "menu", SDL_CONTROLLER_BUTTON_START },
    { "back", SDL_CONTROLLER_BUTTON_BACK },
    { "guide", SDL_CONTROLLER_BUTTON_GUIDE },
#if SDL_VERSION_ATLEAST(2, 0, 14)
    { "share", SDL_CONTROLLER_BUTTON_MISC1 },
    { "touchpad", SDL_CONTROLLER_BUTTON_TOUCHPAD },
#endif
};

// Digital press is derived from the analog axis for lt/rt; SDL has no
// BUTTON_LEFTTRIGGER/RIGHTTRIGGER, only AXIS_TRIGGERLEFT/RIGHT.
constexpr double k_TriggerDigitalThreshold = 0.5;

} // namespace

ControllerManager* ControllerManager::s_Instance = nullptr;

ControllerManager::ControllerManager(QObject* parent)
    : QObject(parent)
    , m_PollTimer(new QTimer(this))
    , m_Started(false)
{
    m_PollTimer->setInterval(POLL_INTERVAL_MS);
    connect(m_PollTimer, &QTimer::timeout, this, &ControllerManager::poll);
}

ControllerManager::~ControllerManager()
{
    stop();
}

ControllerManager*
ControllerManager::get()
{
    if (s_Instance == nullptr) {
        s_Instance = new ControllerManager();
    }

    return s_Instance;
}

void
ControllerManager::start()
{
    if (m_Started) {
        return;
    }

    // Refcounted by SDL; safe alongside SdlGamepadKeyNavigation's own
    // Init/Quit of the same subsystem.
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "ControllerManager: SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) failed: %s",
                     SDL_GetError());
        return;
    }

    m_Started = true;

    // Populate synchronously so the first QML paint isn't empty.
    poll();

    m_PollTimer->start();
}

void
ControllerManager::stop()
{
    if (!m_Started) {
        return;
    }

    m_PollTimer->stop();
    m_Started = false;

    for (const ControllerState& state : std::as_const(m_Controllers)) {
        SDL_GameControllerClose(state.handle);
    }
    m_Controllers.clear();

    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);

    emit controllersChanged();
}

QVariantList
ControllerManager::controllers() const
{
    QVariantList list;

    for (const ControllerState& state : m_Controllers) {
        list.append(toVariant(state));
    }

    return list;
}

QVariantMap
ControllerManager::controllerSnapshot(int deviceIndex) const
{
    QVariantMap map;

    if (deviceIndex < 0 || deviceIndex >= m_Controllers.size()) {
        return map;
    }

    const ControllerState& state = m_Controllers.at(deviceIndex);
    map.insert(QStringLiteral("buttons"), state.buttons);
    map.insert(QStringLiteral("axes"), state.axes);
    return map;
}

QVariantList
ControllerManager::controllerSlots() const
{
    QVariantList list;

    for (const ControllerState& state : m_Controllers) {
        QVariantMap entry;
        entry.insert(QStringLiteral("deviceId"), state.deviceId);
        entry.insert(QStringLiteral("slot"), state.slot);
        list.append(entry);
    }

    return list;
}

bool
ControllerManager::assignSlot(int deviceId, int slot)
{
    for (ControllerState& state : m_Controllers) {
        if (state.deviceId == deviceId) {
            state.slot = slot;
            emit controllersChanged();
            return true;
        }
    }

    return false;
}

void
ControllerManager::renumberSlots()
{
    for (int i = 0; i < m_Controllers.size(); i++) {
        m_Controllers[i].slot = i;
    }

    emit controllersChanged();
}

void
ControllerManager::poll()
{
    if (!m_Started) {
        return;
    }

    // Refreshes SDL's cached joystick/controller state. This does NOT drain
    // the SDL event queue (see header comment) - it's the same call
    // SdlGamepadKeyNavigation uses for the same reason.
    SDL_JoystickUpdate();

    QSet<SDL_JoystickID> seen;
    bool topologyChanged = false;

    int numJoysticks = SDL_NumJoysticks();
    for (int i = 0; i < numJoysticks; i++) {
        if (!SDL_IsGameController(i)) {
            continue;
        }

        SDL_JoystickID instanceId = SDL_JoystickGetDeviceInstanceID(i);
        if (instanceId < 0) {
            continue;
        }

        seen.insert(instanceId);

        if (indexOfInstance(instanceId) >= 0) {
            continue;
        }

        SDL_GameController* gc = SDL_GameControllerOpen(i);
        if (gc == nullptr) {
            continue;
        }

        ControllerState state;
        state.handle = gc;
        state.instanceId = instanceId;
        state.deviceId = i;
        state.slot = m_Controllers.size();
        state.name = QString::fromUtf8(SDL_GameControllerName(gc));
        if (state.name.isEmpty()) {
            state.name = QStringLiteral("Unknown Controller");
        }
        state.family = detectFamily(gc);
        state.path = controllerPath(gc, state.name);

        m_Controllers.append(state);
        topologyChanged = true;
    }

    for (int i = m_Controllers.size() - 1; i >= 0; i--) {
        if (!seen.contains(m_Controllers[i].instanceId)) {
            SDL_GameControllerClose(m_Controllers[i].handle);
            m_Controllers.removeAt(i);
            topologyChanged = true;
        }
    }

    if (topologyChanged) {
        emit controllersChanged();
    }

    for (int i = 0; i < m_Controllers.size(); i++) {
        updateLiveState(m_Controllers[i]);
        emit controllerLiveUpdate(i);
    }
}

QString
ControllerManager::detectFamily(SDL_GameController* gc) const
{
    switch (SDL_GameControllerGetType(gc)) {
    case SDL_CONTROLLER_TYPE_XBOX360:
    case SDL_CONTROLLER_TYPE_XBOXONE:
        return QStringLiteral("xbox");
    case SDL_CONTROLLER_TYPE_PS3:
    case SDL_CONTROLLER_TYPE_PS4:
    case SDL_CONTROLLER_TYPE_PS5:
        return QStringLiteral("playstation");
    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO:
#if SDL_VERSION_ATLEAST(2, 24, 0)
    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
#endif
        return QStringLiteral("nintendo");
    default:
        break;
    }

    // SDL2 has no SDL_CONTROLLER_TYPE_STEAM; mirror the VID check
    // app/streaming/input/gamepad.cpp uses for LI_CTYPE_STEAM.
    if (SDL_GameControllerGetVendor(gc) == 0x28de) {
        return QStringLiteral("steam");
    }

    // Name heuristics as a last resort (generic HID pads, older SDL builds
    // whose gamecontrollerdb entry doesn't set a type).
    QString name = QString::fromUtf8(SDL_GameControllerName(gc)).toLower();
    if (name.contains(QStringLiteral("xbox"))) {
        return QStringLiteral("xbox");
    }
    if (name.contains(QStringLiteral("playstation")) || name.contains(QStringLiteral("dualshock")) ||
        name.contains(QStringLiteral("dualsense")) || name.contains(QStringLiteral("ps3")) ||
        name.contains(QStringLiteral("ps4")) || name.contains(QStringLiteral("ps5"))) {
        return QStringLiteral("playstation");
    }
    if (name.contains(QStringLiteral("switch")) || name.contains(QStringLiteral("nintendo")) ||
        name.contains(QStringLiteral("joy-con"))) {
        return QStringLiteral("nintendo");
    }
    if (name.contains(QStringLiteral("steam"))) {
        return QStringLiteral("steam");
    }

    return QStringLiteral("generic");
}

QString
ControllerManager::controllerPath(SDL_GameController* gc, const QString& fallbackName) const
{
#if SDL_VERSION_ATLEAST(2, 24, 0)
    const char* path = SDL_GameControllerPath(gc);
    if (path != nullptr && path[0] != '\0') {
        return QString::fromUtf8(path);
    }
#endif

    SDL_JoystickGUID guid = SDL_JoystickGetGUID(SDL_GameControllerGetJoystick(gc));
    char guidStr[64] = {};
    SDL_JoystickGetGUIDString(guid, guidStr, sizeof(guidStr));
    return fallbackName + QStringLiteral(":") + QString::fromLatin1(guidStr);
}

void
ControllerManager::updateLiveState(ControllerState& state) const
{
    state.buttons.clear();
    for (const DigitalButtonMapping& mapping : k_DigitalButtons) {
        state.buttons.insert(QString::fromLatin1(mapping.logicalName),
                              SDL_GameControllerGetButton(state.handle, mapping.sdlButton) != 0);
    }

    state.axes.clear();
    state.axes.insert(QStringLiteral("leftx"),
                       qBound(-1.0, SDL_GameControllerGetAxis(state.handle, SDL_CONTROLLER_AXIS_LEFTX) / 32768.0, 1.0));
    state.axes.insert(QStringLiteral("lefty"),
                       qBound(-1.0, SDL_GameControllerGetAxis(state.handle, SDL_CONTROLLER_AXIS_LEFTY) / 32768.0, 1.0));
    state.axes.insert(QStringLiteral("rightx"),
                       qBound(-1.0, SDL_GameControllerGetAxis(state.handle, SDL_CONTROLLER_AXIS_RIGHTX) / 32768.0, 1.0));
    state.axes.insert(QStringLiteral("righty"),
                       qBound(-1.0, SDL_GameControllerGetAxis(state.handle, SDL_CONTROLLER_AXIS_RIGHTY) / 32768.0, 1.0));

    double triggerL = qBound(0.0, SDL_GameControllerGetAxis(state.handle, SDL_CONTROLLER_AXIS_TRIGGERLEFT) / 32767.0, 1.0);
    double triggerR = qBound(0.0, SDL_GameControllerGetAxis(state.handle, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) / 32767.0, 1.0);
    state.axes.insert(QStringLiteral("triggerl"), triggerL);
    state.axes.insert(QStringLiteral("triggerr"), triggerR);

    // Synthesized digital press for the trigger entries in the fixed
    // logical button vocabulary (see header comment).
    state.buttons.insert(QStringLiteral("lt"), triggerL > k_TriggerDigitalThreshold);
    state.buttons.insert(QStringLiteral("rt"), triggerR > k_TriggerDigitalThreshold);
}

QVariantMap
ControllerManager::toVariant(const ControllerState& state) const
{
    QVariantMap map;
    map.insert(QStringLiteral("deviceId"), state.deviceId);
    map.insert(QStringLiteral("instanceId"), static_cast<qlonglong>(state.instanceId));
    map.insert(QStringLiteral("slot"), state.slot);
    map.insert(QStringLiteral("name"), state.name);
    map.insert(QStringLiteral("path"), state.path);
    map.insert(QStringLiteral("family"), state.family);
    map.insert(QStringLiteral("connected"), state.handle != nullptr);
    map.insert(QStringLiteral("buttons"), state.buttons);
    map.insert(QStringLiteral("axes"), state.axes);
    return map;
}

int
ControllerManager::indexOfInstance(SDL_JoystickID instanceId) const
{
    for (int i = 0; i < m_Controllers.size(); i++) {
        if (m_Controllers[i].instanceId == instanceId) {
            return i;
        }
    }

    return -1;
}
