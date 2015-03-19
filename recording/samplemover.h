#ifndef SAMPLEMOVER_H
#define SAMPLEMOVER_H

#include <QObject>
#include <memory>
#include <cstdint>
#include <portaudio.h>
#include <atomic>

#include "external/pa_ringbuffer.h"
#include "error/provider.h"

class QTemporaryFile;


namespace Recording {

class SampleMover : public QObject
{
    Q_OBJECT
public:
    using ErrorType = Error::Provider::ErrorType;

    explicit SampleMover(uint64_t samplesAlreadyRecorded, QObject *parent = 0);

    //! this function is explicitly thread-safe
    uint64_t getRecordedSampleCount();



signals:
    /*!
     * \brief  Errors related to the choice of audio device(s).
     *
     * Possible device errors should be displayed next to the device selection box
     * and are assumed to be resolvable by selecting another audio device.
     */
    void deviceError(ErrorType severity, const QString& message1, const QString& message2);

    /*!
     * \brief  Errors related to the overall recording process
     *
     * Errors related to the overall recording process can range from a warning about
     * dropped samples to a message about an emergency shutdown of the recording process.
     *
     * These errors are supposed to be displayed right next to other recording indicators.
     */
    void recordingError(ErrorType severity, const QString& message1, const QString& message2);

    void levelMeterUpdate(int16_t left, int16_t right);
    void timeUpdate(uint64_t recorded_samples);
    void recordingStateChanged(bool recording, uint64_t total_recorded_sample_count);
    void newRecordingFile(const QString& filePath, uint64_t starting_from_sample_count);
    void canRecordChanged(bool canRecord);
    void canMonitorChanged(bool canMonitor);
    void latencyChanged(double min, double max, double value);

public slots:
    void setInputDevice(PaDeviceIndex device = paNoDevice);
    void setMonitorDevice(PaDeviceIndex device = paNoDevice);
    void setConfiguredLatency(double latency);
    void startRecording();
    void stopRecording();
    void setRecordingState(bool);
    void setMonitorEnabled(bool);
    void setAudioDataDir(const QString& dir);

private slots:
    void reportState();

private:
    static int stream_callback(const void *input,
                               void *output,
                               unsigned long frameCount,
                               const PaStreamCallbackTimeInfo *timeInfo,
                               PaStreamCallbackFlags statusFlags,
                               void *userData);

    bool recordingState() {
        return recording_flag != RecordingState::STOP_ACCEPTED;
    }

    //! (Re)open the recording stream after a device has been changed
    void reopenStream();

    //! Called when critical things like writing the sound file go bananas
    //! Will request the callback to stop recording, then drain the buffer
    //! and silently roll back the counter to the number of samples that
    //! was successfully saved to disk.
    void emergencyShutdown();

    //! Will try to open and set the recording file. Returns whether it succeeds.
    bool openRecordingFile();

    PaStream *stream = nullptr;

    Error::Provider *deviceErrorProvider    = nullptr;
    Error::Provider *recordingErrorProvider = nullptr;

    // The devices - if we have to reopen the stream because one device changed,
    // we need to know the other device we just selected before
    int input_device   = paNoDevice;
    int monitor_device = paNoDevice;

    // We configure just one latency shared among both devices
    // This isn't really correct, but it makes life easier for users
    double m_configured_latency = 1000000000000;

    // The ringbuffer is conveniently lock-free
    PaUtilRingBuffer ringbuffer;
    char             ringbuffer_data[2<<20]; // roughly 5 seconds of data

    // These are std::atomic so both the portaudio callback.
    // And regular class members can safely access them.
    std::atomic<int16_t> level_l { 0 }; // = 0 is not well liked at all :(
    std::atomic<int16_t> level_r { 0 };

    // The temporary level calculations are only done by the callback
    // and not supposed to be expected or interfered with by the
    // other running code
    int16_t temp_level_l = 0;
    int16_t temp_level_r = 0;
    uint64_t temp_level_sample_count = 0; // reset the level meter after 2000 samples == 22fps

    // According to my calculations, we could record
    // about 13 billion years without hitting an overflow.
    // But, after 3 years, we'd have to switch the file so we
    // don't hit the ntfs 16TB limit. The FAT32 4GB limit
    // forces us to switch the file after roughly 6.5h.
    // Most unix file systems are comparable to NTFS (ext3)
    // or even better (ext4, btrfs, ...)
    std::atomic<uint64_t> recorded_sample_count { 0 };
    uint64_t              written_sample_count = 0;
    uint64_t              current_file_started_at = 0;

    // This is a bit tricky and inherently dangerous:
    // We need to communicate with the recording thread in a non-blocking
    // way to make it start or stop sending samples to us (and, more important,
    // stop the sample counter). This is achieved by setting/reading flags
    // in an atomic way. To make it work as safely as possible, there is a clear
    // protocol:
    //
    //  * At the beginning, the flag is set to STOP_ACCEPTED. The callback
    //    guarantees that it will neither touch the counter nor the ringbuffer.
    //  * When the main thread wants the callback to start recording samples, it will
    //    prepare the counter and ringbuffer (if needed) and set the flag to START.
    //    The callback is now free to send samples and increase the counter at it's will.
    //  * When the main thread wants the callback to stop sending samples, it will set the
    //    the STOP_REQUESTED flag. Then it waits (spinlocks, and hopes that this doesn't ever
    //    block the callback thread) until the callback sets the flag to STOP_ACCEPTED.
    //    (see above for the guarantees the callback makes for STOP_ACCEPTED)
    //    The main thread may now drain the ringbuffer or do other funny stuff while waiting.
    //
    // Lock-free programming is hard, so I might have overlooked some gotchas :/
    // Well, we kinda assume that a std::atomic is lock-free for enums, too.
    enum class RecordingState { START, STOP_REQUESTED, STOP_ACCEPTED};
    std::atomic<RecordingState> recording_flag { RecordingState::STOP_ACCEPTED };

    std::atomic<bool> monitor_enabled { false };

    std::atomic<bool> dropping_samples { false };

    QTemporaryFile *currentFile          = nullptr;
    QString         currentDataDirectory = QString();

    bool m_canRecord = false;
    void setCanRecord(bool can)
    {
        if (can != m_canRecord) {
            m_canRecord = can;
            emit canRecordChanged(can);
        }
    }
    bool m_canMonitor = false;
    void setCanMonitor(bool can)
    {
        if (can != m_canMonitor) {
            m_canMonitor = can;
            emit canMonitorChanged(can);
        }
    }

public:
    ~SampleMover();
};

} // namespace Recording

#endif // SAMPLEMOVER_H
