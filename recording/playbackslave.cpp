#include "playbackslave.h"
#include "error/provider.h"
#include "error/simulation.h"
#include "recording/trackdataaccessor.h"
#include "util/misc.h"

#include <string.h>
#include <QTimer>
#include <QDebug>

namespace {
    void fillRingBuffer(Recording::TrackDataAccessor *accessor, PaUtilRingBuffer *buffer)
    {
        void *data_1;
        void *data_2;
        ring_buffer_size_t size_1;
        ring_buffer_size_t size_2;

        auto samplesToBeWritten = PaUtil_GetRingBufferWriteRegions(buffer, std::numeric_limits<ring_buffer_size_t>::max(), &data_1, &size_1, &data_2, &size_2);

        accessor->readDataGuaranteed((char *)data_1, size_1);
        accessor->readDataGuaranteed((char *)data_2, size_2);

        PaUtil_AdvanceRingBufferWriteIndex(buffer, samplesToBeWritten);
    }
}

namespace Recording {

PlaybackSlave::PlaybackSlave(QObject *parent) :
    QObject(parent)
{
    Pa_Initialize();

    PaUtil_InitializeRingBuffer(&m_ringbuffer, 4, sizeof(m_ringbuffer_data)/4, m_ringbuffer_data);

    m_errorProvider = new Error::Provider(this);

    QTimer *t = new QTimer(this);
    t->setInterval(40);
    QObject::connect(t, &QTimer::timeout, this, &PlaybackSlave::onTimerTick);
    t->start();
}

PlaybackSlave::~PlaybackSlave()
{
    Pa_CloseStream(m_stream);

    Pa_Terminate();
}

void PlaybackSlave::insertNewSampleSource(TrackDataAccessor *accessor, uint64_t startPosition)
{
    qDebug() << "Inserting new sample source, gonna start at" << startPosition;

    if (m_playbackState)
        setPlaybackState(false);

    if (m_accessor)
        delete m_accessor;

    m_accessor = accessor;
    accessor->setParent(this);

    QObject::connect(m_accessor->errorProvider(), &Error::Provider::error, m_errorProvider, &Error::Provider::setError);

    m_trackLength = accessor->getLength();
    seek(startPosition);
}

void PlaybackSlave::seek(uint64_t position)
{
    bool playbackRunning = m_playbackState;

    if (playbackRunning)
        setPlaybackState(false);

    m_position = position;

    emit positionChanged(position);

    if (playbackRunning)
        setPlaybackState(true);
}

void PlaybackSlave::setPlaybackState(bool playing, bool fireChangeEvent)
{
    Util::SignalBlocker raiiSignalBlock(this, !fireChangeEvent);

    if (m_playbackState == playing)
        return;

    if (playing) {
        // start stream
        qDebug() << "Starting stream at" << m_position;

        if (m_stream != nullptr)
            Pa_CloseStream(m_stream);

        if (m_monitorDevice == paNoDevice) {
            m_errorProvider->setError(Error::Provider::ErrorType::Error,
                                      tr("No monitor device selected"),
                                      tr("Got to the configuration page and select a monitor device where the track will be played."));

            emit playbackStateChanged(m_playbackState = false);

            return;
        }

        if (!m_accessor) {
            m_errorProvider->setError(Error::Provider::ErrorType::Error,
                                      tr("Track unknown to player"),
                                      tr("This is a programmer error."));

            emit playbackStateChanged(m_playbackState = false);

            return;
        }

        // fill the ringbuffer
        PaUtil_FlushRingBuffer(&m_ringbuffer);
        m_accessor->seek(m_position);

        fillRingBuffer(m_accessor, &m_ringbuffer);

        // get the stream running
        PaStreamParameters monitor_params;
        const PaDeviceInfo *info;
        PaError err;

        info = Pa_GetDeviceInfo(m_monitorDevice);

        if (!info) {
            m_errorProvider->setError(Error::Provider::ErrorType::Error,
                                      tr("Invalid monitor device seleted"),
                                      tr("It is a programmer mistake to let this happen."));

            emit playbackStateChanged(m_playbackState = false);

            return;
        }

        monitor_params.device = m_monitorDevice;
        monitor_params.channelCount = 2;
        monitor_params.sampleFormat = paInt16;
        monitor_params.hostApiSpecificStreamInfo = nullptr;
        monitor_params.suggestedLatency = info->defaultHighOutputLatency;

        err = Pa_OpenStream(&m_stream, nullptr, &monitor_params, 44100, paFramesPerBufferUnspecified, paNoFlag, stream_callback, this);

        if (Error::Simulation::SIMULATE("openAudioStream")) {
            m_errorProvider->simulate(Error::Provider::ErrorType::Error, "openAudioStream");
        } else if (err != paNoError) {
            m_errorProvider->setError(Error::Provider::ErrorType::Error, tr("Could not open device(s)"), Pa_GetErrorText(err));
        } else {
            Pa_StartStream(m_stream);
            m_errorProvider->clearError();
            m_playbackState = true;
        }

        emit playbackStateChanged(m_playbackState);
    } else {
        qDebug() << "Stopping stream after having played" << m_position << "samples";

        // stop stream
        Pa_AbortStream(m_stream);
        Pa_CloseStream(m_stream);

        m_stream = nullptr;
        m_playbackState = false;

        emit playbackStateChanged(false);

        if (m_position >= m_trackLength)
            seek(0);
    }
}

void PlaybackSlave::setMonitorDevice(PaDeviceIndex index)
{
    bool currentlyPlaying = m_playbackState;

    if (currentlyPlaying)
        setPlaybackState(false);

    m_monitorDevice = index;

    if (currentlyPlaying)
        setPlaybackState(true);
}

void PlaybackSlave::onTimerTick()
{
    if (!m_stream)
        return;

    // sync stream state
    bool playing = Pa_IsStreamActive(m_stream);
    if (m_playbackState != playing)
        this->setPlaybackState(playing);

    if (!m_accessor->eof()) {
        fillRingBuffer(m_accessor, &m_ringbuffer);
    }

    emit positionChanged(m_position);
}

int PlaybackSlave::stream_callback(const void *input,
                                   void *output,
                                   unsigned long frameCount,
                                   const PaStreamCallbackTimeInfo *timeInfo,
                                   PaStreamCallbackFlags statusFlags,
                                   void *userData)
{
    PlaybackSlave *slave = (PlaybackSlave*)userData;
    Q_UNUSED(input);
    Q_UNUSED(timeInfo);

    if (statusFlags & paOutputOverflow)
        qWarning() << "FIXME: Output overflowed";

    if (statusFlags & paOutputUnderflow)
        qWarning() << "FIXME: Output underflowed (WTF?)";

   auto samplesPlayed = PaUtil_ReadRingBuffer(&slave->m_ringbuffer, output, frameCount);

    slave->m_position += samplesPlayed;

    if ((unsigned long)samplesPlayed != frameCount && slave->m_position < slave->m_trackLength) {
        auto difference = frameCount - samplesPlayed;

        memset((char *)output + samplesPlayed*4, 0, difference*4);

        if (slave->m_position < slave->m_trackLength) {
            qWarning() << "FIXME: Not enough samples in buffer. Should we abort? Ignore? Retry?";
        }
    }


    return slave->m_position < slave->m_trackLength ? paContinue : paComplete;
}

} // namespace Recording
