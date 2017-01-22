#include "coordinator.h"

#include "levelcalculator.h"
#include "lameencoderstream.h"

#include <QTimer>
#include <QDebug>
#include <QDateTime>
#include <QFile>
#include <QDir>
#include <QtMath>

#include <cmath>
#include <cstring>

namespace
{
    const int SAMPLE_RATE = 48000;
    const int SAMPLE_SIZE = 2 * sizeof(float); // 2*4 bytes

    PaStreamParameters ourInputParams(PaDeviceIndex index, const PaDeviceInfo *info)
    {
        PaStreamParameters p = {};

        if (index != paNoDevice)
        {
            p.device = index;
            p.channelCount = 2;
            p.sampleFormat = paFloat32;
            p.suggestedLatency = info->defaultHighInputLatency;
        }

        return p;
    }

    PaStreamParameters ourOutputParams(PaDeviceIndex index, const PaDeviceInfo *info)
    {
        PaStreamParameters p = {};

        if (index != paNoDevice)
        {
            p.device = index;
            p.channelCount = 2;
            p.sampleFormat = paFloat32;
            p.suggestedLatency = info->defaultHighOutputLatency;
        }

        return p;
    }
}

namespace Recording {

Coordinator::Coordinator(QObject *parent) : QObject(parent)
{
    Pa_Initialize();

    m_levelCalculator = new LevelCalculator(this);

    QObject::connect(m_levelCalculator, &LevelCalculator::levelUpdate, this, &Coordinator::handleLevelUpdate);

    int BUFFER_SIZE = qNextPowerOfTwo(SAMPLE_RATE * 5);
    m_ringbufferData = std::make_unique<float[]>(SAMPLE_SIZE * BUFFER_SIZE / sizeof(float));
    PaUtil_InitializeRingBuffer(&m_ringbuffer, SAMPLE_SIZE, BUFFER_SIZE, m_ringbufferData.get());

    QTimer *t = new QTimer(this);
    t->setInterval(40);
    QObject::connect(t, &QTimer::timeout, this, &Coordinator::processAudio);
    t->start();
}

Coordinator::~Coordinator()
{
    stopAudio();

    Pa_Terminate();
}

bool Coordinator::isSupportedInput(PaDeviceIndex index, const PaDeviceInfo *info)
{
    auto p = ourInputParams(index, info);
    return paNoError == Pa_IsFormatSupported(&p, nullptr, SAMPLE_RATE);
}

bool Coordinator::isSupportedOutput(PaDeviceIndex index, const PaDeviceInfo *info)
{
    auto p = ourOutputParams(index, info);
    return paNoError == Pa_IsFormatSupported(nullptr, &p, SAMPLE_RATE);
}

void Coordinator::setRecordingDevice(const PaDeviceIndex &device)
{
    if (device != m_recordingDev)
    {
        stopAudio();

        m_recordingDev = device;
        emit recordingDeviceChanged(m_recordingDev);

        startAudio();
    }
}

void Coordinator::setMonitorDevice(const PaDeviceIndex &device)
{
    if (device != m_monitorDev)
    {
        stopAudio();

        m_monitorDev = device;
        emit monitorDeviceChanged(m_monitorDev);

        startAudio();
    }
}

void Coordinator::setMonitorEnabled(bool enabled)
{
    if (m_monitorEnabled.exchange(enabled) != enabled)
    {
        emit monitorEnabledChanged(m_monitorEnabled);
    }
}

void Coordinator::startRecording()
{
    if (isRecording())
        stopRecording();

    if (!m_audioStream || m_recordingDev == paNoDevice)
    {
        error(tr("You can't start recoding until your audio input works"));
        stopRecording();
        return;
    }

    QString filename = QString(tr("Recording from %2.mp3"))
            .arg(QDateTime::currentDateTime().toString(tr("yyyy-MM-dd hhmm t")));

    m_mp3FileStream = new QFile(QDir::cleanPath(QString("%1/%2").arg(m_saveDir).arg(filename)));
    if (!m_mp3FileStream->open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        error(tr("MP3: Could not open file %1: %2").arg(filename, m_mp3FileStream->errorString()));
        stopRecording();
        return;
    }

    QString track = tr("Recording from %1").arg(QDateTime::currentDateTime().toString(Qt::DefaultLocaleLongDate));

    m_mp3Stream = new LameEncoderStream(this);
    QObject::connect(m_mp3Stream, &LameEncoderStream::error, this, &Coordinator::error);

    if (!m_mp3Stream->init(m_mp3ArtistName, track, 192, m_mp3FileStream))
    {
        stopRecording();
        return;
    }

    emit recordingChanged(isRecording());
}

void Coordinator::stopRecording()
{
    if (m_mp3Stream)
    {
        m_mp3Stream->close();
        delete m_mp3Stream;
        m_mp3Stream = nullptr;
    }

    if (m_mp3FileStream)
    {
        m_mp3FileStream->close();
        delete m_mp3FileStream;
        m_mp3FileStream = nullptr;
    }

    m_samplesSaved = 0;
    emit recordingChanged(isRecording());
}

void Coordinator::setRecording(bool record)
{
    if (record)
        startRecording();
    else
        stopRecording();
}

void Coordinator::setVolumeFactor(float factor)
{
    m_volumeFactor.store(factor);
    emit volumeFactorChanged(m_volumeFactor);
}

void Coordinator::handleLevelUpdate(float levelL, float levelR)
{
    emit statusUpdate(levelL, levelR, isRecording(), samplesRecorded());
}

void Coordinator::setSaveDir(const QString &dir)
{
    if (m_saveDir != dir)
    {
        m_saveDir = dir;
        emit saveDirChanged(dir);
    }
}

void Coordinator::setMp3ArtistName(const QString &name)
{
    if (m_mp3ArtistName != name)
    {
        m_mp3ArtistName = name;
        emit mp3ArtistNameChanged(name);
    }
}

int Coordinator::audioCallback(const void *inputBuffer, void *outputBuffer,
                                        unsigned long framesPerBuffer,
                                        const PaStreamCallbackTimeInfo */*timeInfo*/,
                                        PaStreamCallbackFlags /*statusFlags*/, void *userData)
{
    Coordinator *self = static_cast<Coordinator*>(userData);

    if (outputBuffer)
    {
        if (inputBuffer && self->m_monitorEnabled.load(std::memory_order_relaxed))
        {
            float factor = self->m_volumeFactor.load(std::memory_order_relaxed);
            auto *in  = static_cast<const float*>(inputBuffer);
            auto *out = static_cast<float*>(outputBuffer);
            for (unsigned long i = 0; i < framesPerBuffer; ++i)
            {
                out[2*i + 0] = in[2*i + 0] * factor;
                out[2*i + 1] = in[2*i + 1] * factor;
            }
        }
        else
        {
            std::memset(outputBuffer, 0, framesPerBuffer * SAMPLE_SIZE);
        }
    }

    if (inputBuffer)
    {
        PaUtil_WriteRingBuffer(&self->m_ringbuffer, inputBuffer, framesPerBuffer);
    }

    return paContinue;
}

void Coordinator::stopAudio()
{
    if (!m_audioStream)
        return;

    Pa_StopStream(m_audioStream);
    Pa_CloseStream(m_audioStream);
    m_audioStream = nullptr;
}

void Coordinator::startAudio()
{
    if (m_audioStream)
        stopAudio();

    if (m_recordingDev == paNoDevice && m_monitorDev == paNoDevice)
        return;

    PaStreamParameters inp = ourInputParams(m_recordingDev, Pa_GetDeviceInfo(m_recordingDev));
    PaStreamParameters outp = ourOutputParams(m_monitorDev, Pa_GetDeviceInfo(m_monitorDev));

    PaError err = Pa_OpenStream(&m_audioStream,
                                m_recordingDev != paNoDevice ? &inp : nullptr,
                                m_monitorDev != paNoDevice ? &outp : nullptr,
                                SAMPLE_RATE, paFramesPerBufferUnspecified, paNoFlag,
                                &Coordinator::audioCallback, this);
    if (err != paNoError)
    {
        emit error(Pa_GetErrorText(err));
        return;
    }

    err = Pa_StartStream(m_audioStream);
    if (err != paNoError)
    {
        emit error(Pa_GetErrorText(err));
    }
    else
    {
        emit error(QString());
    }
}

void Coordinator::processAudio()
{
    float buffer[2048];
    qint64 nsamples = 0;
    while ((nsamples = PaUtil_ReadRingBuffer(&m_ringbuffer, buffer, sizeof(buffer)/m_ringbuffer.elementSizeBytes)))
    {
        // do level calculation
        m_levelCalculator->processAudio(buffer, nsamples);

        if (m_mp3Stream)
        {
            m_mp3Stream->writeAudio(buffer, nsamples);

            m_samplesSaved += nsamples;
        }
    }
}

} // namespace Recording
