#pragma once

#include <QTimer>
#include <QEvent>
#include <QString>

#include "SDL_compat.h"

#include "settings/streamingpreferences.h"

class SdlGamepadKeyNavigation : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString inputMode READ inputMode NOTIFY inputModeChanged)
    Q_PROPERTY(QString controllerFamily READ controllerFamily NOTIFY controllerFamilyChanged)

public:
    static SdlGamepadKeyNavigation* get();
    SdlGamepadKeyNavigation(StreamingPreferences* prefs);

    ~SdlGamepadKeyNavigation();

    Q_INVOKABLE void enable();

    Q_INVOKABLE void disable();

    Q_INVOKABLE void notifyWindowFocus(bool hasFocus);

    Q_INVOKABLE void setUiNavMode(bool settingsMode);
    Q_INVOKABLE void setStreamOverlayMode(bool enabled);

    Q_INVOKABLE int getConnectedGamepads();

    QString inputMode() const { return m_InputMode; }
    QString controllerFamily() const { return m_ControllerFamily; }

signals:
    void inputModeChanged();

    void controllerFamilyChanged();
protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void sendKey(QEvent::Type type, Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    void setInputMode(const QString& mode);

    void updateControllerFamily(SDL_GameController* controller);
    void updateTimerState();

private slots:
    void onPollingTimerFired();

private:
    static SdlGamepadKeyNavigation* s_Instance;
    StreamingPreferences* m_Prefs;
    QTimer* m_PollingTimer;
    QList<SDL_GameController*> m_Gamepads;
    bool m_Enabled;
    bool m_UiNavMode;
    bool m_StreamOverlayMode;
    bool m_FirstPoll;
    bool m_HasFocus;
    QString m_InputMode;
    bool m_SendingControllerKey;
    Uint32 m_LastAxisNavigationEventTime;
    QString m_ControllerFamily;
};
