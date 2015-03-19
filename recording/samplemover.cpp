#include "recording/samplemover.h"

#include "error/provider.h"
#include "error/simulation.h"

#include "util/portaudio.h"

#include <QDebug>
#include <QTimer>
#include <QtEndian>
#include <QTemporaryFile>
#include <portaudio.h>
#include <atomic>

int
Recording::SampleMover::stream_callback(const void *input,
                             void *output,
                             unsigned long frameCount,
                             const PaStreamCallbackTimeInfo *timeInfo,
                             PaStreamCallbackFlags statusFlags,
                             void *userData)
{
    Recording::SampleMover *mover = (Recording::SampleMover*)userData;
    (void)timeInfo; // at least on linux, the time info is pretty much worthless

    if (statusFlags & paInputOverflow)
        qWarning() << "FIXME: Input overflowed";

    if (statusFlags & paInputUnderflow)
        qWarning() << "FIXME: Input underflowed (WTF?)";

    // well, we don't really care enough for the monitor to handle its status codes

    if (input) {
        int16_t *samples = (int16_t *)input;

        // FIXME: should we do the level calculation outside the callback?
        // is the simplified code worth the added latency?
        for (unsigned long i = 0; i < frameCount; ++i) {
            mover->m_tempLevelL = qMax(mover->m_tempLevelL, qAbs(qFromLittleEndian(samples[2*i + 0])));
            mover->m_tempLevelR = qMax(mover->m_tempLevelR, qAbs(qFromLittleEndian(samples[2*i + 1])));
        }

        mover->m_tempLevelSampleCount += frameCount;

        if (mover->m_tempLevelSampleCount >= 2000) {
            mover->m_level_l = mover->m_tempLevelL;
            mover->m_level_r = mover->m_tempLevelR;
            mover->m_tempLevelSampleCount = 0;
            mover->m_tempLevelL = 0;
            mover->m_tempLevelR = 0;
        }
    }

    if (output && input && mover->m_monitorEnabled) {
        memcpy(output, input, frameCount * 4);
    } else if (output) {
        memset(output, 0, frameCount * 4);
    }

    RecordingState rec = mover->m_recordingFlag;
    switch (rec) {
    case RecordingState::START:
        if (input) {
            auto samples_written = PaUtil_WriteRingBuffer(&mover->m_ringbuffer, input, frameCount);

            if ((intmax_t)samples_written != (intmax_t)frameCount)
                qDebug() << "Dropped samples because ringbuffer is full!? Tried to save " << frameCount << ", saved " << samples_written;

            mover->m_recordedSampleCount += samples_written;
        } else {
            // we still have the obligation to fill the ringbuffer, so we'll fill it with silence
            //HACK: If we don't have input, we can be certain to have an output pointer that is already filled
            // with zeroes from the memset above. We use this as source for the PaUtil_WriteRingbuffer call.
            auto samples_written = PaUtil_WriteRingBuffer(&mover->m_ringbuffer, output, frameCount);

            if ((intmax_t)samples_written != (intmax_t)frameCount) {
                qDebug() << "Dropped samples because ringbuffer is full!? Tried to save " << frameCount << ", saved " << samples_written;
                mover->m_droppingSamples = true;
            } else {
                mover->m_droppingSamples = false;
            }

            mover->m_recordedSampleCount += samples_written;
        }
        break;
    case RecordingState::STOP_REQUESTED:
        mover->m_recordingFlag = RecordingState::STOP_ACCEPTED;

        // fall through
    case RecordingState::STOP_ACCEPTED:

        // do nothing
        break;
    default:
        Q_UNREACHABLE();
    }

    return paContinue;
}

Recording::SampleMover::SampleMover(uint64_t samplesAlreadyRecorded, QObject *parent) :
    QObject(parent), m_recordedSampleCount(samplesAlreadyRecorded)
{
    Pa_Initialize();

    PaUtil_InitializeRingBuffer(&m_ringbuffer, 4, 1<<18, m_ringbuffer_data);

    m_deviceErrorProvider    = new Error::Provider(this);
    m_recordingErrorProvider = new Error::Provider(this);

    QObject::connect(m_deviceErrorProvider,    &Error::Provider::error, this, &SampleMover::deviceError);
    QObject::connect(m_recordingErrorProvider, &Error::Provider::error, this, &SampleMover::recordingError);

    setInputDevice();

    QTimer *t = new QTimer(this);
    t->setInterval(40);
    QObject::connect(t, &QTimer::timeout, this, &Recording::SampleMover::reportState);
    t->start();
}

void
Recording::SampleMover::reportState()
{
    if (m_recordingFlag != RecordingState::STOP_ACCEPTED) {
        int32_t buffer[1024];
        ring_buffer_size_t read;

        while ((read = PaUtil_ReadRingBuffer(&m_ringbuffer, buffer, 1024))) {
            auto written = m_currentFile->write((char *)buffer, (qint64)read*4);

            if (written > 0)
                m_writtenSampleCount += written/4;

            if (written != read*4 || Error::Simulation::SIMULATE("writeAudioFile")) {
                if (written == read*4)
                    m_recordingErrorProvider->simulate(Error::Provider::ErrorType::Error, "writeAudioFile");
                else
                    m_recordingErrorProvider->setError(Error::Provider::ErrorType::Error,
                                                     tr("Could not write recorded data to file"),
                                                     tr("While writing to `%1': %2").arg(m_currentFile->fileName()).arg(m_currentFile->errorString()));

                emergencyShutdown();
                return;
            }
        }

        // check if we are in danger of hitting the FAT32 file size limit
        if (m_writtenSampleCount*4 >= 2000000000 || Error::Simulation::SIMULATE("switchAudioFile")) {
            if (!openRecordingFile())
                emergencyShutdown();
        }
    }

    if (this->m_droppingSamples)
        m_recordingErrorProvider->setError(Error::Provider::ErrorType::TemporaryWarning,
                                         tr("Audio data needed to be dropped"),
                                         tr("Your computer cannot process audio data fast enough. Please reduce your system load or buy new hardware."));

    // report conditions to the user interface
    emit levelMeterUpdate(m_level_l, m_level_r);
    emit timeUpdate(m_recordedSampleCount);
}

void
Recording::SampleMover::reopenStream()
{
    if (m_stream) {
        Pa_StopStream(m_stream);
        Pa_CloseStream(m_stream);
        m_stream = nullptr;
        m_level_l = 0;
        m_level_r = 0;
        emit levelMeterUpdate(m_level_l, m_level_r);
    }

    setCanRecord(false);
    setCanMonitor(false);

    PaStreamParameters input_params;
    PaStreamParameters monitor_params;
    const PaDeviceInfo *info;
    PaError err;

    double min_latency = 0;
    double max_latency = std::numeric_limits<double>::max();
    Util::PortAudio::latencyBounds(m_inputDevice, m_monitorDevice, &min_latency, &max_latency);

    if (m_inputDevice != paNoDevice) {
        info = Pa_GetDeviceInfo(m_inputDevice);
        if (!info) {
            m_deviceErrorProvider->setError(Error::Provider::ErrorType::Error, tr("Invalid input device seleted"), tr("It is a programmer mistake to let this happen."));
            return;
        }

        input_params.device = m_inputDevice;
        input_params.channelCount = 2;
        input_params.sampleFormat = paInt16;
        input_params.hostApiSpecificStreamInfo = nullptr;
        input_params.suggestedLatency = qBound(info->defaultLowInputLatency, m_configuredLatency, info->defaultHighInputLatency);
    }

    if (m_monitorDevice != paNoDevice) {
        info = Pa_GetDeviceInfo(m_monitorDevice);

        if (!info) {
            m_deviceErrorProvider->setError(Error::Provider::ErrorType::Error, tr("Invalid monitor device seleted"), tr("It is a programmer mistake to let this happen."));
            return;
        }

        monitor_params.device = m_monitorDevice;
        monitor_params.channelCount = 2;
        monitor_params.sampleFormat = paInt16;
        monitor_params.hostApiSpecificStreamInfo = nullptr;
        monitor_params.suggestedLatency = qBound(info->defaultLowOutputLatency, m_configuredLatency, info->defaultHighOutputLatency);
    }

    PaStreamParameters *input_params_ptr   = m_inputDevice   != paNoDevice ? &input_params   : nullptr;
    PaStreamParameters *monitor_params_ptr = m_monitorDevice != paNoDevice ? &monitor_params : nullptr;

    if (input_params_ptr || monitor_params_ptr) {
        err = Pa_OpenStream(&m_stream, input_params_ptr, monitor_params_ptr, 44100, paFramesPerBufferUnspecified, paNoFlag, stream_callback, this);

        if (Error::Simulation::SIMULATE("openAudioStream")) {
            m_deviceErrorProvider->simulate(Error::Provider::ErrorType::Error, "openAudioStream");
        } else if (err != paNoError) {
            m_deviceErrorProvider->setError(Error::Provider::ErrorType::Error, tr("Could not open device(s)"), Pa_GetErrorText(err));
        } else {
            Pa_StartStream(m_stream);
            m_deviceErrorProvider->clearError();
        }
    }

    if (m_inputDevice != paNoDevice && m_stream && Pa_IsStreamActive(m_stream))
        setCanRecord(true);
    if (m_monitorDevice != paNoDevice && m_stream && Pa_IsStreamActive(m_stream))
        setCanMonitor(true);
    if (m_stream && Pa_IsStreamActive(m_stream))
        latencyChanged(min_latency, max_latency, qBound(min_latency, m_configuredLatency, max_latency));
}

void Recording::SampleMover::emergencyShutdown()
{
    if (m_stream && Pa_IsStreamActive(m_stream)) {
        auto expected = RecordingState::START;
        if (m_recordingFlag.compare_exchange_strong(expected, RecordingState::STOP_REQUESTED)) {
            // now spinlock until the stop has been accepted
            // this should only take a few milliseconds.
            //FIXME: is this guaranteed not to block the callback thread?
            while (m_recordingFlag != RecordingState::STOP_ACCEPTED)
                ; //spin

            // drain the ringbuffer
            int32_t buffer[1024];
            ring_buffer_size_t read;

            while ((read = PaUtil_ReadRingBuffer(&m_ringbuffer, buffer, 1024)))
                ;

            // silently roll back the recorded counter
            m_recordedSampleCount = m_currentFileStartedAt + m_writtenSampleCount;
        } else {
            qWarning() << "Trying to stop recording while it is not running.";
            return;
        }
    } else {
        // If we have no currently active stream, for whatever reason, we obviously can't ask it to stop.
        m_recordingFlag = RecordingState::STOP_ACCEPTED;
    }

    if (m_currentFile) {
        m_currentFile->close();
        delete m_currentFile;
        m_currentFile = nullptr;
    }

    emit timeUpdate(m_recordedSampleCount);
    emit recordingStateChanged(false, m_recordedSampleCount);
}

bool Recording::SampleMover::openRecordingFile()
{
    if (m_currentFile) {
        m_currentFile->close();
        delete m_currentFile;
    }

    m_currentFile = new QTemporaryFile(QString("%1/recorder-data-XXXXXX").arg(m_currentDataDirectory), this);
    m_currentFile->setAutoRemove(false);
    m_currentFile->open();

    if (m_currentFile->error()) {
        m_recordingErrorProvider->setError(Error::Provider::ErrorType::Error,
                                         tr("Couldn't open file to save recorded data"),
                                         tr("While opening `%1': %2").arg(m_currentFile->fileName()).arg(m_currentFile->errorString()));

        return false;
    } else if (Error::Simulation::SIMULATE("openAudioDataFile")) {
        m_recordingErrorProvider->simulate(Error::Provider::ErrorType::Error, "openAudioDataFile");

        return false;
    } else {
        qDebug() << "Old file began at " << m_currentFileStartedAt << ", has written " << m_writtenSampleCount << "samples";

        m_currentFileStartedAt += m_writtenSampleCount;
        m_writtenSampleCount = 0;

        emit newRecordingFile(m_currentFile->fileName(), m_currentFileStartedAt);

        return true;
    }
}

void
Recording::SampleMover::setInputDevice(PaDeviceIndex device)
{
    m_inputDevice = device;

    reopenStream();
}

void
Recording::SampleMover::setMonitorDevice(PaDeviceIndex device)
{
    m_monitorDevice = device;

    reopenStream();
}

void Recording::SampleMover::setConfiguredLatency(double latency)
{
    if (m_configuredLatency == latency)
        return;

    m_configuredLatency = latency;

    reopenStream();
}

void
Recording::SampleMover::setMonitorEnabled(bool enabled)
{
    m_monitorEnabled = enabled;
}

void Recording::SampleMover::setAudioDataDir(const QString &dir)
{
    m_currentDataDirectory = dir;

    if (m_recordingFlag == RecordingState::STOP_ACCEPTED)
        return;

    // reopen a file in the new directory right now because the user might have
    // switched the directory on the fly to evade space problems.
    if (!openRecordingFile())
        emergencyShutdown();
}

void
Recording::SampleMover::startRecording()
{
    if (m_recordingFlag != RecordingState::STOP_ACCEPTED) {
        qWarning() << "Trying to start recording while it is already running.";
    } else {
        PaUtil_FlushRingBuffer(&m_ringbuffer);

        if (m_currentFileStartedAt + m_writtenSampleCount != m_recordedSampleCount) {
            qWarning() << "Discrepancy between recorded and written sample count. Correcting.";
            m_writtenSampleCount = 0;
            m_currentFileStartedAt = m_recordedSampleCount;
        }

        if (openRecordingFile()) {
            m_recordingFlag = RecordingState::START;

            emit recordingStateChanged(true, m_recordedSampleCount);
        } else {
            emit recordingStateChanged(false, m_recordedSampleCount);
        }
    }
}

void
Recording::SampleMover::stopRecording()
{
    if (m_stream && Pa_IsStreamActive(m_stream)) {
        auto expected = RecordingState::START;
        if (m_recordingFlag.compare_exchange_strong(expected, RecordingState::STOP_REQUESTED)) {
            // now spinlock until the stop has been accepted
            // this should only take a few milliseconds.
            //FIXME: is this guaranteed not to block the callback thread?
            while (m_recordingFlag != RecordingState::STOP_ACCEPTED)
                ; //spin

            // drain the ringbuffer to file
            int32_t buffer[1024];
            ring_buffer_size_t read;

            while ((read = PaUtil_ReadRingBuffer(&m_ringbuffer, buffer, 1024))) {
                auto written = m_currentFile->write((char *)buffer, (qint64)read * 4);

                if (written > 0)
                    m_writtenSampleCount += written/4;

                if (written != read * 4) {
                    m_recordingErrorProvider->setError(Error::Provider::ErrorType::Error,
                                                     tr("Could not write recorded data to file"),
                                                     tr("While writing to `%1': %2").arg(m_currentFile->fileName()).arg(m_currentFile->errorString()));

                    emergencyShutdown();
                    return;
                }
            }

            qDebug() << "Recording successfully stopped";

        } else {
            qWarning() << "Trying to stop recording while it is not running.";
            return;
        }
    } else {
        // If we have no currently active stream, for whatever reason, we obviously can't ask it to stop.
        m_recordingFlag = RecordingState::STOP_ACCEPTED;
    }

    m_currentFile->close();
    delete m_currentFile;
    m_currentFile = nullptr;

    qDebug() << "Written samples: " << m_currentFileStartedAt + m_writtenSampleCount << "Recorded samples: " << m_recordedSampleCount;

    emit recordingStateChanged(false, m_recordedSampleCount);
}

void Recording::SampleMover::setRecordingState(bool recording)
{
    if (recording == recordingState())
        return;

    if (recording)
        startRecording();
    else
        stopRecording();
}

Recording::SampleMover::~SampleMover()
{
    if (m_stream) {
        Pa_StopStream(m_stream);
        Pa_CloseStream(m_stream);
    }

    Pa_Terminate();
}

uint64_t
Recording::SampleMover::recordedSampleCount()
{
    return m_recordedSampleCount;
}
