//
// SPDX-FileCopyrightText: Lunaframe Client Contributors
//
// SPDX-License-Identifier: GPL-3.0-only
//
// Jochona: controller-first M2 spine (proposal §5.3, §6.7). Owns SDL2
// gamepad discovery and live per-controller input state for the QML
// Controller Manager screen (ControllerManagerView.qml).
//
// This is deliberately state-polling only: poll() calls SDL_JoystickUpdate()
// (refreshes SDL's cached device state) then reads button/axis state
// directly with SDL_GameControllerGetButton()/GetAxis(), and detects
// hotplug by diffing SDL_NumJoysticks() against the previously-open set.
// It never calls SDL_PeepEvents()/SDL_PollEvent(), so it never competes for
// SDL_CONTROLLERBUTTONDOWN/UP or SDL_CONTROLLERDEVICEADDED/REMOVED events
// with SdlGamepadKeyNavigation (UI nav, see app/gui/sdlgamepadkeynavigation.cpp)
// or with the streaming Session's own gamepad input path
// (app/streaming/input/gamepad.cpp). Those two remain the only consumers of
// the SDL controller event queue.
//
// controllers (Q_PROPERTY) is a topology snapshot: it only changes (and
// only emits controllersChanged()) when a controller is added/removed or a
// slot is reassigned. Per-poll button/axis motion is delivered separately
// via controllerLiveUpdate(deviceIndex) + controllerSnapshot(deviceIndex),
// so a 30Hz input visualization strip never forces QML to rebuild the whole
// controllers list/model on every tick.
//
#pragma once

#include <QObject>
#include <QTimer>
#include <QList>
#include <QSet>
#include <QVariantList>
#include <QVariantMap>
#include <QString>

#include "SDL_compat.h"

class ControllerManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVariantList controllers READ controllers NOTIFY controllersChanged)

public:
    Q_INVOKABLE static ControllerManager*
    get();

    QVariantList
    controllers() const;

    // Live per-poll state for one controller (index into the controllers
    // list), independent of the controllers property/notify so the input
    // visualization strip can refresh at 30Hz without a full list rebuild.
    // Returns {} for an out-of-range index.
    Q_INVOKABLE QVariantMap
    controllerSnapshot(int deviceIndex) const;

    // M2 slot-assignment stubs: slots are advisory ordering metadata today
    // (persisted only in memory). Wiring a slot to the actual streaming
    // input path is moonlight-common-c/session territory, out of scope here.
    Q_INVOKABLE QVariantList
    controllerSlots() const;

    Q_INVOKABLE bool
    assignSlot(int deviceId, int slot);

    Q_INVOKABLE void
    renumberSlots();

    // Starts/stops SDL_INIT_GAMECONTROLLER + the 30Hz poll timer. Safe to
    // call repeatedly; mirrors SdlGamepadKeyNavigation::enable()/disable()
    // so overlapping lifetimes (this screen open during a session, etc.)
    // don't double-init or double-quit the subsystem.
    Q_INVOKABLE void
    start();

    Q_INVOKABLE void
    stop();

signals:
    void
    controllersChanged();

    // Emitted once per poll tick per currently-connected controller with
    // its list index. Handlers should pull fresh data via
    // controllerSnapshot(deviceIndex) rather than re-reading `controllers`.
    void
    controllerLiveUpdate(int deviceIndex);

private slots:
    void
    poll();

private:
    struct ControllerState
    {
        SDL_GameController* handle = nullptr;
        SDL_JoystickID instanceId = -1;
        int deviceId = -1;
        int slot = -1;
        QString name;
        QString path;
        QString family;
        QVariantMap buttons;
        QVariantMap axes;
    };

    ControllerManager(QObject* parent = nullptr);

    ~ControllerManager() override;

    Q_DISABLE_COPY(ControllerManager)

    QString
    detectFamily(SDL_GameController* gc) const;

    // Stable-ish identity key for ControllerProfileStore. Prefers
    // SDL_GameControllerPath() (SDL >= 2.24, tracks the physical device
    // path); older SDL builds fall back to "name:GUID", which is stable
    // per controller model but not per physical unit.
    QString
    controllerPath(SDL_GameController* gc, const QString& fallbackName) const;

    void
    updateLiveState(ControllerState& state) const;

    QVariantMap
    toVariant(const ControllerState& state) const;

    int
    indexOfInstance(SDL_JoystickID instanceId) const;

    static ControllerManager* s_Instance;

    QTimer* m_PollTimer;
    bool m_Started;
    QList<ControllerState> m_Controllers;
};
