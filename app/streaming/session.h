#pragma once

#include <QSemaphore>
#include <QQuickWindow>

#include <Limelight.h>
#include <opus_multistream.h>
#include "settings/streamingpreferences.h"
#include "input/input.h"
#include "video/decoder.h"
#include "audio/renderers/renderer.h"
#include "video/overlaymanager.h"
#include "backend/adapters/hostcapabilities.h"
#include <memory>
#include <atomic>

class SupportedVideoFormatList : public QList<int>
{
public:
    operator int() const
    {
        int value = 0;

        for (const int v : *this) {
            value |= v;
        }

        return value;
    }

    void
    removeByMask(int mask)
    {
        int i = 0;
        while (i < this->length()) {
            if (this->value(i) & mask) {
                this->removeAt(i);
            }
            else {
                i++;
            }
        }
    }

    void
    deprioritizeByMask(int mask)
    {
        QList<int> deprioritizedList;

        int i = 0;
        while (i < this->length()) {
            if (this->value(i) & mask) {
                deprioritizedList.append(this->takeAt(i));
            }
            else {
                i++;
            }
        }

        this->append(std::move(deprioritizedList));
    }

    int maskByServerCodecModes(int serverCodecModes)
    {
        int mask = 0;

        const QMap<int, int> mapping = {
            {SCM_H264, VIDEO_FORMAT_H264},
            {SCM_H264_HIGH8_444, VIDEO_FORMAT_H264_HIGH8_444},
            {SCM_HEVC, VIDEO_FORMAT_H265},
            {SCM_HEVC_MAIN10, VIDEO_FORMAT_H265_MAIN10},
            {SCM_HEVC_REXT8_444, VIDEO_FORMAT_H265_REXT8_444},
            {SCM_HEVC_REXT10_444, VIDEO_FORMAT_H265_REXT10_444},
            {SCM_AV1_MAIN8, VIDEO_FORMAT_AV1_MAIN8},
            {SCM_AV1_MAIN10, VIDEO_FORMAT_AV1_MAIN10},
            {SCM_AV1_HIGH8_444, VIDEO_FORMAT_AV1_HIGH8_444},
            {SCM_AV1_HIGH10_444, VIDEO_FORMAT_AV1_HIGH10_444},
        };

        for (QMap<int, int>::const_iterator it = mapping.cbegin(); it != mapping.cend(); ++it) {
            if (serverCodecModes & it.key()) {
                mask |= it.value();
                serverCodecModes &= ~it.key();
            }
        }

        // Make sure nobody forgets to update this for new SCM values
        SDL_assert(serverCodecModes == 0);

        int val = *this;
        return val & mask;
    }
};

class Session : public QObject
{
    Q_OBJECT

    friend class SdlInputHandler;
    friend class DeferredSessionCleanupTask;
    friend class AsyncConnectionStartThread;

public:
    explicit Session(NvComputer* computer, NvApp& app, StreamingPreferences *preferences = nullptr);
    virtual ~Session();

    Q_INVOKABLE bool initialize(QQuickWindow* qtWindow);
    Q_INVOKABLE void start();
    Q_INVOKABLE void interrupt();
    Q_PROPERTY(QStringList launchWarnings MEMBER m_LaunchWarnings NOTIFY launchWarningsChanged);
    Q_PROPERTY(double sessionVolumeDb READ sessionVolumeDb
               WRITE setSessionVolumeDb NOTIFY sessionVolumeDbChanged)
    double sessionVolumeDb() const;
    Q_INVOKABLE void setSessionVolumeDb(double db);
    Q_PROPERTY(QString hostUuid READ hostUuid CONSTANT)
    Q_PROPERTY(int appId READ appId CONSTANT)
    QString hostUuid() const;
    int appId() const { return m_App.id; }
    Q_PROPERTY(QString libraryEntryId READ libraryEntryId CONSTANT)
    Q_PROPERTY(QVariantMap currentSettings READ currentSettings
               NOTIFY currentSettingsChanged)
    Q_PROPERTY(bool performanceOverlayEnabled READ performanceOverlayEnabled
               WRITE setPerformanceOverlayEnabled
               NOTIFY currentSettingsChanged)
    Q_PROPERTY(bool hostVolumeAvailable READ hostVolumeAvailable CONSTANT)
    // hostVolumeAvailable gates whether this Client holds host.volume.read
    // for this Host (i.e. Host Volume can be fetched and displayed);
    // hostVolumeWritable additionally gates host.volume.write (i.e. the
    // slider can actually change it). Neither implies Session Volume,
    // which stays local to this Client either way.
    Q_PROPERTY(bool hostVolumeWritable READ hostVolumeWritable CONSTANT)
    Q_PROPERTY(bool hostVolumeLoaded READ hostVolumeLoaded NOTIFY hostVolumeStateChanged)
    Q_PROPERTY(int hostVolumeMin READ hostVolumeMin NOTIFY hostVolumeStateChanged)
    Q_PROPERTY(int hostVolumeMax READ hostVolumeMax NOTIFY hostVolumeStateChanged)
    Q_PROPERTY(int hostVolumeCurrent READ hostVolumeCurrent NOTIFY hostVolumeStateChanged)
    Q_PROPERTY(QString hostVolumeErrorText READ hostVolumeErrorText NOTIFY hostVolumeErrorChanged)
    QString libraryEntryId() const;
    QVariantMap currentSettings() const;
    bool performanceOverlayEnabled() const;
    bool hostVolumeAvailable() const;
    bool hostVolumeWritable() const;
    bool hostVolumeLoaded() const;
    int hostVolumeMin() const;
    int hostVolumeMax() const;
    int hostVolumeCurrent() const;
    QString hostVolumeErrorText() const;
    // Fetches the Host's current Host Volume state over pinned mTLS
    // without blocking the calling (GUI) thread; hostVolumeStateChanged
    // or hostVolumeErrorChanged reports the outcome. A no-op if this
    // Client doesn't hold host.volume.read for this Host.
    Q_INVOKABLE void refreshHostVolume();
    // Requests the Host set its Host Volume to level (0-100, clamped
    // to [hostVolumeMin, hostVolumeMax] once known). A no-op if this
    // Client doesn't hold host.volume.write for this Host.
    Q_INVOKABLE void setHostVolume(int level);
    Q_INVOKABLE void setPerformanceOverlayEnabled(bool enabled);
    void requestSessionSettings();
    Q_INVOKABLE void closeSessionSettings();
    Q_INVOKABLE void applySessionSettings(
            const QVariantMap& restartPatch,
            const QString& saveScope);

    // A reconnect is a fresh Session resolved against the current Client
    // Device and Display Context. Session is single-use.
    Q_INVOKABLE Session* createReconnectSession();
    Q_INVOKABLE void notifyDisplayContextChanged(
            const QString& displayName);
    void requestDisplayReconnect();

    static
    void getDecoderInfo(SDL_Window* window,
                        bool& isHardwareAccelerated, bool& isFullScreenOnly,
                        bool& isHdrSupported, QSize& maxResolution);

    static Session* get()
    {
        return s_ActiveSession;
    }

    Overlay::OverlayManager& getOverlayManager()
    {
        return m_OverlayManager;
    }

    void flushWindowEvents();

    void setShouldExit(bool quitHostApp = false);

signals:
    void stageStarting(QString stage);

    void stageFailed(QString stage, int errorCode, QString failingPorts);

    // Jochona M3 (session resilience): raw termination code + failing ports
    // so QML can render its own plain-language guidance instead of parsing
    // Session::displayLaunchError()'s pre-formatted text. Fires for every
    // termination reason, including graceful (errorCode 0), so listeners
    // can distinguish an intentional quit from a real failure.
    void connectionTerminated(int errorCode, QString failingPorts);

    void connectionStarted();

    void displayLaunchError(QString text);

    void quitStarting();

    void sessionFinished(int portTestResult);

    // Emitted after sessionFinished() when the session is ready to be destroyed
    void readyForDeletion();

    void launchWarningsChanged();
    void sessionVolumeDbChanged();
    void displayReconnectRequested();
    void sessionSettingsRequested();
    void currentSettingsChanged();
    void hostVolumeStateChanged();
    void hostVolumeErrorChanged();

private:
    void exec();

    bool startConnectionAsync();

    bool validateLaunch(SDL_Window* testWindow);

    void emitLaunchWarning(QString text);

    // First-launch (or first-launch-at-this-mode) Encoder Tuple preflight
    // (ADR-0011): called from startConnectionAsync() when
    // HostCapabilities::selectEncoderTuple() found no cached tuple for
    // this exact already-locked wire format. Returns a default-
    // constructed (empty id) EncoderTuple and sets error on any failure;
    // never substitutes a different format.
    HostCapabilities::EncoderTuple probeEncoderTupleForLaunch(
            int videoFormat, bool virtualDisplay, QString& error);

    // Issues one async, pinned-mTLS Host Volume GET (isWrite == false) or
    // PUT (isWrite == true, level 0-100) without blocking the calling
    // thread. Reports the outcome via hostVolumeStateChanged() or
    // hostVolumeErrorChanged().
    void performHostVolumeRequest(bool isWrite, int level);

    bool populateDecoderProperties(SDL_Window* window);

    IAudioRenderer* createAudioRenderer(const POPUS_MULTISTREAM_CONFIGURATION opusConfig);

    bool initializeAudioRenderer();

    bool testAudio(int audioConfiguration);

    int getAudioRendererCapabilities(int audioConfiguration);

    void getWindowDimensions(int& x, int& y,
                             int& width, int& height);

    void toggleFullscreen();

    void notifyMouseEmulationMode(bool enabled);

    void updateOptimalWindowDisplayMode();

    enum class DecoderAvailability {
        None,
        Software,
        Hardware
    };

    static
    DecoderAvailability getDecoderAvailability(SDL_Window* window,
                                               StreamingPreferences::VideoDecoderSelection vds,
                                               int videoFormat, int width, int height, int frameRate);

    static
    bool chooseDecoder(StreamingPreferences::VideoDecoderSelection vds,
                       StreamingPreferences::RendererSelection renderer,
                       SDL_Window* window, int videoFormat, int width, int height,
                       int frameRate, bool enableVsync, bool enableFramePacing,
                       bool testOnly,
                       IVideoDecoder*& chosenDecoder);

    static
    void clStageStarting(int stage);

    static
    void clStageFailed(int stage, int errorCode);

    static
    void clConnectionTerminated(int errorCode);

    static
    void clLogMessage(const char* format, ...);

    static
    void clRumble(unsigned short controllerNumber, unsigned short lowFreqMotor, unsigned short highFreqMotor);

    static
    void clConnectionStatusUpdate(int connectionStatus);

    static
    void clSetHdrMode(bool enabled);

    static
    void clRumbleTriggers(uint16_t controllerNumber, uint16_t leftTrigger, uint16_t rightTrigger);

    static
    void clSetMotionEventState(uint16_t controllerNumber, uint8_t motionType, uint16_t reportRateHz);

    static
    void clSetControllerLED(uint16_t controllerNumber, uint8_t r, uint8_t g, uint8_t b);

    static
    void clSetAdaptiveTriggers(uint16_t controllerNumber, uint8_t eventFlags, uint8_t typeLeft, uint8_t typeRight, uint8_t *left, uint8_t *right);

    static
    int arInit(int audioConfiguration,
               const POPUS_MULTISTREAM_CONFIGURATION opusConfig,
               void* arContext, int arFlags);

    static
    void arCleanup();

    static
    void arDecodeAndPlaySample(char* sampleData, int sampleLength);

    static
    int drSetup(int videoFormat, int width, int height, int frameRate, void*, int);

    static
    void drCleanup();

    static
    int drSubmitDecodeUnit(PDECODE_UNIT du);

    std::unique_ptr<StreamingPreferences> m_OwnedPreferences;
    StreamingPreferences* m_Preferences;
    bool m_IsFullScreen;
    SupportedVideoFormatList m_SupportedVideoFormats; // Sorted in order of descending priority
    STREAM_CONFIGURATION m_StreamConfig;
    DECODER_RENDERER_CALLBACKS m_VideoCallbacks;
    AUDIO_RENDERER_CALLBACKS m_AudioCallbacks;
    NvComputer* m_Computer;
    NvApp m_App;
    SDL_Window* m_Window;
    IVideoDecoder* m_VideoDecoder;
    SDL_mutex* m_DecoderLock;
    bool m_AudioDisabled;
    bool m_AudioMuted;
    std::atomic<double> m_SessionVolumeDb;
    std::atomic<float> m_AudioVolumeGain;
    Uint32 m_FullScreenFlag;
    QQuickWindow* m_QtWindow;
    std::atomic<bool> m_SessionSettingsOpen;
    QVariantMap m_ReconnectSessionPatch;
    bool m_UnexpectedTermination;
    SdlInputHandler* m_InputHandler;
    int m_MouseEmulationRefCount;
    int m_FlushingWindowEventsRef;
    QStringList m_LaunchWarnings;
    bool m_ShouldExit;

    // Host Volume live state. Each request creates its own short-lived
    // QNetworkAccessManager (matching NvHTTP/HostProber's per-call
    // pattern) so overlapping GET/PUT calls (e.g. a fast slider drag)
    // never share pinning handler state; requests are fully async
    // (connect+lambda), so they never block the GUI thread.
    bool m_HostVolumeLoaded;
    int m_HostVolumeMin;
    int m_HostVolumeMax;
    int m_HostVolumeCurrent;
    QString m_HostVolumeErrorText;
    quint64 m_HostVolumeRequestSerial;

    bool m_AsyncConnectionSuccess;
    int m_PortTestResults;

    int m_ActiveVideoFormat;
    int m_ActiveVideoWidth;
    int m_ActiveVideoHeight;
    int m_ActiveVideoFrameRate;

    OpusMSDecoder* m_OpusDecoder;
    IAudioRenderer* m_AudioRenderer;
    OPUS_MULTISTREAM_CONFIGURATION m_ActiveAudioConfig;
    OPUS_MULTISTREAM_CONFIGURATION m_OriginalAudioConfig;
    int m_AudioSampleCount;
    Uint32 m_DropAudioEndTime;

    Overlay::OverlayManager m_OverlayManager;

    static CONNECTION_LISTENER_CALLBACKS k_ConnCallbacks;
    static Session* s_ActiveSession;
    static QSemaphore s_ActiveSessionSemaphore;
};
