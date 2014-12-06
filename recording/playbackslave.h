#ifndef RECORDING_PLAYBACKSLAVE_H
#define RECORDING_PLAYBACKSLAVE_H

#include <QObject>
#include <atomic>
#include <portaudio.h>
#include "external/pa_ringbuffer.h"

namespace Error {
    class Provider;
}

namespace Recording {

class TrackDataAccessor;

/*!
 * \brief The PlaybackSlave class is responsible for playing audio samples to the monitor device
 */
class PlaybackSlave : public QObject
{
    Q_OBJECT
public:
    explicit PlaybackSlave(QObject *parent = 0);
    ~PlaybackSlave();

    Q_INVOKABLE
    uint64_t position() const
    {
        return m_position;
    }

    uint64_t trackLength() const
    {
        return m_trackLength;
    }

    bool     playbackState() const
    {
        return m_playbackState;
    }

    const Error::Provider *errorProvider() const
    {
        return m_errorProvider;
    }

signals:
    void positionChanged(uint64_t position);
    void playbackStateChanged(bool currentlyPlaying);

public slots:
    //! will take ownership of the accessor
    void insertNewSampleSource(TrackDataAccessor *accessor, uint64_t startPosition);
    void seek(uint64_t position);
    void setPlaybackState(bool playing, bool fireChangeEvent);
    void setPlaybackState(bool playing) { setPlaybackState(playing, true); }
    void setMonitorDevice(PaDeviceIndex index = paNoDevice);

private slots:
    void onTimerTick();

private:
    bool     m_playbackState = false;

    PaDeviceIndex      m_monitorDevice = paNoDevice;
    TrackDataAccessor *m_accessor      = nullptr;

    // This value is incremented for every sample handed off to portaudio
    std::atomic<uint64_t> m_position {0};
    std::atomic<uint64_t> m_trackLength {0};

    Error::Provider *m_errorProvider = nullptr;

    // The ringbuffer is conveniently lock-free
    PaUtilRingBuffer m_ringbuffer;
    char             m_ringbuffer_data[1<<20];

    PaStream        *m_stream = nullptr;

    static int stream_callback(const void *input,
                               void *output,
                               unsigned long frameCount,
                               const PaStreamCallbackTimeInfo *timeInfo,
                               PaStreamCallbackFlags statusFlags,
                               void *userData);
};

} // namespace Recording

#endif // RECORDING_PLAYBACKSLAVE_H
