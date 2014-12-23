#include "recording/samplemover.h"

#include "error/provider.h"
#include "error/simulation.h"

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

    // well, we don't really care enough for the monitor to handle it's status codes

    if (input) {
        int16_t *samples = (int16_t *)input;

        // FIXME: should we do the level calculation outside the callback?
        // is the simplified code worth the added latency?
        for (unsigned long i = 0; i < frameCount; ++i) {
            mover->temp_level_l = qMax(mover->temp_level_l, qAbs(qFromLittleEndian(samples[2*i + 0])));
            mover->temp_level_r = qMax(mover->temp_level_r, qAbs(qFromLittleEndian(samples[2*i + 1])));
        }

        mover->temp_level_sample_count += frameCount;

        if (mover->temp_level_sample_count >= 2000) {
            mover->level_l = mover->temp_level_l;
            mover->level_r = mover->temp_level_r;
            mover->temp_level_sample_count = 0;
            mover->temp_level_l = 0;
            mover->temp_level_r = 0;
        }
    }

    if (output && input && mover->monitor_enabled) {
        memcpy(output, input, frameCount * 4);
    } else if (output) {
        memset(output, 0, frameCount * 4);
    }

    RecordingState rec = mover->recording_flag;
    switch (rec) {
    case RecordingState::START:
        if (input) {
            auto samples_written = PaUtil_WriteRingBuffer(&mover->ringbuffer, input, frameCount);

            if ((intmax_t)samples_written != (intmax_t)frameCount)
                qDebug() << "Dropped samples because ringbuffer is full!? Tried to save " << frameCount << ", saved " << samples_written;

            mover->recorded_sample_count += samples_written;
        } else {
            // we still have the obligation to fill the ringbuffer, so we'll fill it with silence
            //HACK: If we don't have input, we can be certain to have an output pointer that is already filled
            // with zeroes from the memset above. We use this as source for the PaUtil_WriteRingbuffer call.
            auto samples_written = PaUtil_WriteRingBuffer(&mover->ringbuffer, output, frameCount);

            if ((intmax_t)samples_written != (intmax_t)frameCount) {
                qDebug() << "Dropped samples because ringbuffer is full!? Tried to save " << frameCount << ", saved " << samples_written;
                mover->dropping_samples = true;
            } else {
                mover->dropping_samples = false;
            }

            mover->recorded_sample_count += samples_written;
        }
        break;
    case RecordingState::STOP_REQUESTED:
        mover->recording_flag = RecordingState::STOP_ACCEPTED;

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
    QObject(parent), recorded_sample_count(samplesAlreadyRecorded)
{
    Pa_Initialize();

    PaUtil_InitializeRingBuffer(&ringbuffer, 4, 1<<18, ringbuffer_data);

    deviceErrorProvider    = new Error::Provider(this);
    recordingErrorProvider = new Error::Provider(this);

    setInputDevice();

    QTimer *t = new QTimer(this);
    t->setInterval(40);
    QObject::connect(t, &QTimer::timeout, this, &Recording::SampleMover::reportState);
    t->start();
}

Error::Provider *
Recording::SampleMover::getDeviceErrorProvider()
{
    return deviceErrorProvider;
}

Error::Provider *Recording::SampleMover::getRecordingErrorProvider()
{
    return recordingErrorProvider;
}

void
Recording::SampleMover::reportState()
{
    if (recording_flag != RecordingState::STOP_ACCEPTED) {
        int32_t buffer[1024];
        ring_buffer_size_t read;

        while ((read = PaUtil_ReadRingBuffer(&ringbuffer, buffer, 1024))) {
            auto written = currentFile->write((char *)buffer, (qint64)read*4);

            if (written > 0)
                written_sample_count += written/4;

            if (written != read*4 || Error::Simulation::SIMULATE("writeAudioFile")) {
                if (written == read*4)
                    recordingErrorProvider->simulate(Error::Provider::ErrorType::Error, "writeAudioFile");
                else
                    recordingErrorProvider->setError(Error::Provider::ErrorType::Error,
                                                     tr("Could not write recorded data to file"),
                                                     tr("While writing to `%1': %2").arg(currentFile->fileName()).arg(currentFile->errorString()));

                emergencyShutdown();
                return;
            }
        }

        // check if we are in danger of hitting the FAT32 file size limit
        if (written_sample_count*4 >= 2000000000 || Error::Simulation::SIMULATE("switchAudioFile")) {
            if (!openRecordingFile())
                emergencyShutdown();
        }
    }

    if (this->dropping_samples)
        recordingErrorProvider->setError(Error::Provider::ErrorType::TemporaryWarning,
                                         tr("Audio data needed to be dropped"),
                                         tr("Your computer cannot process audio data fast enough. Please reduce your system load or buy new hardware."));

    // report conditions to the user interface
    emit levelMeterUpdate(level_l, level_r);
    emit timeUpdate(recorded_sample_count);
}

void
Recording::SampleMover::reopenStream()
{
    if (stream) {
        Pa_StopStream(stream);
        Pa_CloseStream(stream);
        stream = nullptr;
        level_l = 0;
        level_r = 0;
    }

    setCanRecord(false);
    setCanMonitor(false);

    PaStreamParameters input_params;
    PaStreamParameters monitor_params;
    const PaDeviceInfo *info;
    PaError err;

    if (input_device != paNoDevice) {
        info = Pa_GetDeviceInfo(input_device);

        if (!info) {
            deviceErrorProvider->setError(Error::Provider::ErrorType::Error, tr("Invalid input device seleted"), tr("It is a programmer mistake to let this happen."));
            return;
        }

        input_params.device = input_device;
        input_params.channelCount = 2;
        input_params.sampleFormat = paInt16;
        input_params.hostApiSpecificStreamInfo = nullptr;
        input_params.suggestedLatency = info->defaultHighInputLatency;
        //FIXME: We'd really like to use low latency, but if we request that,
        // even the developer machine will go nuts if one tries to play a video
        // in the background. Might be an Alsa/Pulseaudio/Portaudio problem tough,
        // so we should check with a Win7 machine and may decide to ignore the problem.
        // It's a shame Portaudio can't automatically adjust the latency to avoid
        // the CPU utilization on the fly.
    }

    if (monitor_device != paNoDevice) {
        info = Pa_GetDeviceInfo(monitor_device);

        if (!info) {
            deviceErrorProvider->setError(Error::Provider::ErrorType::Error, tr("Invalid monitor device seleted"), tr("It is a programmer mistake to let this happen."));
            return;
        }

        monitor_params.device = monitor_device;
        monitor_params.channelCount = 2;
        monitor_params.sampleFormat = paInt16;
        monitor_params.hostApiSpecificStreamInfo = nullptr;
        monitor_params.suggestedLatency = info->defaultHighOutputLatency;
    }

    PaStreamParameters *input_params_ptr   = input_device   != paNoDevice ? &input_params   : nullptr;
    PaStreamParameters *monitor_params_ptr = monitor_device != paNoDevice ? &monitor_params : nullptr;

    if (input_params_ptr || monitor_params_ptr) {
        err = Pa_OpenStream(&stream, input_params_ptr, monitor_params_ptr, 44100, paFramesPerBufferUnspecified, paNoFlag, stream_callback, this);

        if (Error::Simulation::SIMULATE("openAudioStream")) {
            deviceErrorProvider->simulate(Error::Provider::ErrorType::Error, "openAudioStream");
        } else if (err != paNoError) {
            deviceErrorProvider->setError(Error::Provider::ErrorType::Error, tr("Could not open device(s)"), Pa_GetErrorText(err));
        } else {
            Pa_StartStream(stream);
            deviceErrorProvider->clearError();
        }
    }

    if (input_device != paNoDevice && stream && Pa_IsStreamActive(stream))
        setCanRecord(true);
    if (monitor_device != paNoDevice && stream && Pa_IsStreamActive(stream))
        setCanMonitor(true);
}

void Recording::SampleMover::emergencyShutdown()
{
    if (stream && Pa_IsStreamActive(stream)) {
        auto expected = RecordingState::START;
        if (recording_flag.compare_exchange_strong(expected, RecordingState::STOP_REQUESTED)) {
            // now spinlock until the stop has been accepted
            // this should only take a few milliseconds.
            //FIXME: is this guaranteed not to block the callback thread?
            while (recording_flag != RecordingState::STOP_ACCEPTED)
                ; //spin

            // drain the ringbuffer
            int32_t buffer[1024];
            ring_buffer_size_t read;

            while ((read = PaUtil_ReadRingBuffer(&ringbuffer, buffer, 1024)))
                ;

            // silently roll back the recorded counter
            recorded_sample_count = current_file_started_at + written_sample_count;
        } else {
            qWarning() << "Trying to stop recording while it is not running.";
            return;
        }
    } else {
        // If we have no currently active stream, for whatever reason, we obviously can't ask it to stop.
        recording_flag = RecordingState::STOP_ACCEPTED;
    }

    if (currentFile) {
        currentFile->close();
        delete currentFile;
        currentFile = nullptr;
    }

    emit timeUpdate(recorded_sample_count);
    emit recordingStateChanged(false, recorded_sample_count);
}

bool Recording::SampleMover::openRecordingFile()
{
    if (currentFile) {
        currentFile->close();
        delete currentFile;
    }

    currentFile = new QTemporaryFile(QString("%1/recorder-data-XXXXXX").arg(currentDataDirectory), this);
    currentFile->setAutoRemove(false);
    currentFile->open();

    if (currentFile->error()) {
        recordingErrorProvider->setError(Error::Provider::ErrorType::Error,
                                         tr("Couldn't open file to save recorded data"),
                                         tr("While opening `%1': %2").arg(currentFile->fileName()).arg(currentFile->errorString()));

        return false;
    } else if (Error::Simulation::SIMULATE("openAudioDataFile")) {
        recordingErrorProvider->simulate(Error::Provider::ErrorType::Error, "openAudioDataFile");

        return false;
    } else {
        qDebug() << "Old file began at " << current_file_started_at << ", has written " << written_sample_count << "samples";

        current_file_started_at += written_sample_count;
        written_sample_count = 0;

        emit newRecordingFile(currentFile->fileName(), current_file_started_at);

        return true;
    }
}

void
Recording::SampleMover::setInputDevice(PaDeviceIndex device)
{
    input_device = device;

    reopenStream();
}

void
Recording::SampleMover::setMonitorDevice(PaDeviceIndex device)
{
    monitor_device = device;

    reopenStream();
}

void
Recording::SampleMover::setMonitorEnabled(bool enabled)
{
    monitor_enabled = enabled;
}

void Recording::SampleMover::setAudioDataDir(const QString &dir)
{
    currentDataDirectory = dir;

    if (recording_flag == RecordingState::STOP_ACCEPTED)
        return;

    // reopen a file in the new directory right now because the user might have
    // switched the directory on the fly to evade space problems.
    if (!openRecordingFile())
        emergencyShutdown();
}

void
Recording::SampleMover::startRecording()
{
    if (recording_flag != RecordingState::STOP_ACCEPTED) {
        qWarning() << "Trying to start recording while it is already running.";
    } else {
        PaUtil_FlushRingBuffer(&ringbuffer);

        if (current_file_started_at + written_sample_count != recorded_sample_count) {
            qWarning() << "Discrepancy between recorded and written sample count. Correcting.";
            written_sample_count = 0;
            current_file_started_at = recorded_sample_count;
        }

        if (openRecordingFile()) {
            recording_flag = RecordingState::START;

            emit recordingStateChanged(true, recorded_sample_count);
        } else {
            emit recordingStateChanged(false, recorded_sample_count);
        }
    }
}

void
Recording::SampleMover::stopRecording()
{
    if (stream && Pa_IsStreamActive(stream)) {
        auto expected = RecordingState::START;
        if (recording_flag.compare_exchange_strong(expected, RecordingState::STOP_REQUESTED)) {
            // now spinlock until the stop has been accepted
            // this should only take a few milliseconds.
            //FIXME: is this guaranteed not to block the callback thread?
            while (recording_flag != RecordingState::STOP_ACCEPTED)
                ; //spin

            // drain the ringbuffer to file
            int32_t buffer[1024];
            ring_buffer_size_t read;

            while ((read = PaUtil_ReadRingBuffer(&ringbuffer, buffer, 1024))) {
                auto written = currentFile->write((char *)buffer, (qint64)read * 4);

                if (written > 0)
                    written_sample_count += written/4;

                if (written != read * 4) {
                    recordingErrorProvider->setError(Error::Provider::ErrorType::Error,
                                                     tr("Could not write recorded data to file"),
                                                     tr("While writing to `%1': %2").arg(currentFile->fileName()).arg(currentFile->errorString()));

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
        recording_flag = RecordingState::STOP_ACCEPTED;
    }

    currentFile->close();
    delete currentFile;
    currentFile = nullptr;

    qDebug() << "Written samples: " << current_file_started_at + written_sample_count << "Recorded samples: " << recorded_sample_count;

    emit recordingStateChanged(false, recorded_sample_count);
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
    if (stream) {
        Pa_StopStream(stream);
        Pa_CloseStream(stream);
    }

    Pa_Terminate();
}

uint64_t
Recording::SampleMover::getRecordedSampleCount()
{
    return recorded_sample_count;
}
