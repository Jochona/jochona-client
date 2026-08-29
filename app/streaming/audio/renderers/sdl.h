#pragma once

#include "renderer.h"
#include "SDL_compat.h"
#include <QString>

class SdlAudioRenderer : public IAudioRenderer
{
public:
    explicit SdlAudioRenderer(const QString& requestedDevice = QString());

    virtual ~SdlAudioRenderer();

    virtual bool prepareForPlayback(const OPUS_MULTISTREAM_CONFIGURATION* opusConfig);

    virtual void* getAudioBuffer(int* size);

    virtual bool submitAudio(int bytesWritten);

    virtual AudioFormat getAudioBufferFormat();
    bool usedFallbackDevice() const { return m_UsedFallbackDevice; }
    QString requestedDevice() const { return m_RequestedDevice; }

private:
    SDL_AudioDeviceID m_AudioDevice;
    void* m_AudioBuffer;
    Uint32 m_FrameSize;
    Uint32 m_FrameDurationMs;
    QString m_RequestedDevice;
    bool m_UsedFallbackDevice;
};
