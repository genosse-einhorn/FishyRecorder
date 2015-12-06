#include "flacfileexporter.h"

#include <QIODevice>
#include <QTime>

namespace {

FLAC__StreamEncoderWriteStatus writeToQIODevice(const FLAC__StreamEncoder *encoder,
                                                const FLAC__byte buffer[],
                                                size_t bytes,
                                                unsigned samples,
                                                unsigned current_frame,
                                                void *client_data)
{
    Q_UNUSED(samples);
    Q_UNUSED(current_frame);
    Q_UNUSED(encoder);

    QIODevice *dev = (QIODevice*)client_data;

    if (dev->write((char*)buffer, bytes) == (qint64)bytes)
        return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
    else
        return FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;
}

} // namespace

namespace Export {

FlacFileExporter::FlacFileExporter(QObject *parent) : EncodedFileExporter(parent)
{
}

FlacFileExporter::~FlacFileExporter()
{
    FLAC__stream_encoder_delete(m_encoder);
}

QString FlacFileExporter::fileExtension() const
{
    return "flac";
}

bool FlacFileExporter::beginTrack(QIODevice *output, uint64_t trackLength, const QString &trackName, int trackIndex)
{
    Q_UNUSED(trackLength);
    Q_UNUSED(trackName);
    Q_UNUSED(trackIndex);

    if (m_encoder) // delete it, to make sure it is reset
        FLAC__stream_encoder_delete(m_encoder);

    m_device = output;
    m_encoder = FLAC__stream_encoder_new();

    FLAC__stream_encoder_set_streamable_subset(m_encoder, true);
    FLAC__stream_encoder_set_channels(m_encoder, 2);
    FLAC__stream_encoder_set_bits_per_sample(m_encoder, 16);
    FLAC__stream_encoder_set_sample_rate(m_encoder, 48000);

    // Initialize stuff for dithering algorithm
    qsrand(QTime::currentTime().msec() * QTime::currentTime().second());

    auto status = FLAC__stream_encoder_init_stream(m_encoder, &writeToQIODevice, nullptr, nullptr, nullptr, output);

    if (status == FLAC__STREAM_ENCODER_INIT_STATUS_OK)
        return true;

    m_errorProvider->setError(Error::Provider::ErrorType::Error, tr("FLAC Error"), FLAC__StreamEncoderInitStatusString[status]);
    return false;
}

bool FlacFileExporter::encodeData(const float *buffer, uint64_t numSamples)
{
    // Convert to integer samples, with dithering
    for (; numSamples > 0; --numSamples, buffer += 2) {
        int32_t s[] = { doDither(buffer[0]), doDither(buffer[1]) };

        if (!FLAC__stream_encoder_process_interleaved(m_encoder, s, 1)) {
            auto status = FLAC__stream_encoder_get_state(m_encoder);

            m_errorProvider->setError(Error::Provider::ErrorType::Error, tr("FLAC Error"), FLAC__StreamEncoderStateString[status]);

            return false;
        }
    }

    return true;
}

bool FlacFileExporter::finishTrack()
{
    if (!FLAC__stream_encoder_finish(m_encoder)) {
        auto state = FLAC__stream_encoder_get_state(m_encoder);

        m_errorProvider->setError(Error::Provider::ErrorType::Error, tr("FLAC Error"), FLAC__StreamEncoderStateString[state]);
        return false;
    } else {
        return true;
    }
}

// Dithering algorithm from http://www.musicdsp.org/files/nsdither.txt
int32_t FlacFileExporter::doDither(float in)
{
    int32_t out;
    float tmp;

    in = qBound(-1.0f, in, 1.0f);

    r2 = r1;                             //can make HP-TRI dither by
    r1 = qrand();                        //subtracting previous rand()

    in += s * (s1 + s1 - s2);            //error feedback
    tmp = in + o + d * float(r1 - r2);   //dc offset and dither

    out = int32_t(w * tmp);              //truncate downwards
    if (tmp < 0.0f)
        out--;                           //this is faster than floor()

    s2 = s1;
    s1 = in - wi * float(out);           //error

    return out;
}

} // namespace Export

