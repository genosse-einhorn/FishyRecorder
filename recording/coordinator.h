#ifndef RECORDINGCOORDINATOR_H
#define RECORDINGCOORDINATOR_H

#include <QObject>

#include <portaudio.h>

#include <atomic>
#include <memory>

#include "external/pa_ringbuffer.h"

class QIODevice;
class QTimer;
class QFile;

namespace Recording {

class LameEncoderStream;
class LevelCalculator;

class Coordinator : public QObject
{
    Q_OBJECT
public:
    explicit Coordinator(QObject *parent = 0);
    ~Coordinator();

    PaDeviceIndex recordingDevice() const { return m_recordingDev; }
    PaDeviceIndex monitorDevice() const { return m_monitorDev; }
    bool monitorEnabled() const { return m_monitorEnabled; }

    static bool isSupportedInput(PaDeviceIndex index, const PaDeviceInfo *info);
    static bool isSupportedOutput(PaDeviceIndex index, const PaDeviceInfo *info);

    bool isRecording() const { return m_mp3Stream != nullptr; }
    qint64 samplesRecorded() const { return m_samplesSaved; }

    float volumeFactor() const { return m_volumeFactor; }

    QString saveDir() const { return m_saveDir; }
    QString fileName() const { return m_filename; }
    QString mp3ArtistName() const { return m_mp3ArtistName; }

signals:
    void error(const QString &message);

    void recordingDeviceChanged(const PaDeviceIndex &device);
    void monitorDeviceChanged(const PaDeviceIndex &device);
    void monitorEnabledChanged(bool);

    void volumeFactorChanged(float);

    void saveDirChanged(const QString &dir);
    void mp3ArtistNameChanged(const QString &name);

    void statusUpdate(float levelL, float levelR, bool isRecording, qint64 recordedSamples);
    void recordingChanged(bool isRecording);

public slots:
    void setRecordingDevice(const PaDeviceIndex &device);
    void setMonitorDevice(const PaDeviceIndex &device);
    void setMonitorEnabled(bool);

    void startRecording();
    void stopRecording();
    void setRecording(bool record);

    void setVolumeFactor(float factor);

    void handleLevelUpdate(float levelL, float levelR);

    void setSaveDir(const QString &dir);
    void setMp3ArtistName(const QString &name);

private:
    static int audioCallback(const void *inputBuffer, void *outputBuffer,
                             unsigned long framesPerBuffer,
                             const PaStreamCallbackTimeInfo* timeInfo,
                             PaStreamCallbackFlags statusFlags,
                             void *userData);

    void stopAudio();
    void startAudio();

    void processAudio();

private:
    Recording::LevelCalculator *m_levelCalculator;

    QIODevice *m_mp3FileStream { nullptr };
    Recording::LameEncoderStream *m_mp3Stream { nullptr };

    PaDeviceIndex m_recordingDev { paNoDevice };
    PaDeviceIndex m_monitorDev { paNoDevice };

    std::atomic<float> m_volumeFactor { 1.0f };

    std::atomic_bool m_monitorEnabled { false };

    qint64 m_samplesSaved { 0 };

    QString m_saveDir;
    QString m_filename;
    QString m_mp3ArtistName { "Someone" };

    PaStream *m_audioStream { nullptr };

    std::unique_ptr<float[]> m_ringbufferData;
    PaUtilRingBuffer m_ringbuffer {};
};

} // namespace Recording

#endif // RECORDINGCOORDINATOR_H
