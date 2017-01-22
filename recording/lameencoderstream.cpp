#include "lameencoderstream.h"

#include <lame/lame.h>

#include <QDebug>
#include <memory>

namespace Recording {

LameEncoderStream::LameEncoderStream(QObject *parent)
: QObject(parent), m_lame_gbf(lame_init()), m_device(nullptr)
{}

bool
LameEncoderStream::init(const QString &artist, const QString &trackName,
                        int brate, QIODevice *output)
{
    m_device = output;

    lame_set_quality(m_lame_gbf, 5);
    lame_set_mode(m_lame_gbf, JOINT_STEREO);
    lame_set_brate(m_lame_gbf, brate);
    lame_set_in_samplerate(m_lame_gbf, 48000);
    lame_set_out_samplerate(m_lame_gbf, 48000);

    // ID3 tags are barely documented, but luckily we can read the lame(1) source code...
    id3tag_init(m_lame_gbf);
    id3tag_v2_only(m_lame_gbf);

    if (artist.length()) {
        QByteArray artist_u8 = artist.toUtf8();
        id3tag_set_artist(m_lame_gbf, artist_u8.constData());
    }
    if (trackName.length()) {
        QByteArray trackName_u8 = trackName.toUtf8();
        id3tag_set_title(m_lame_gbf, trackName_u8.constData());
    }

    if (lame_init_params(m_lame_gbf) < 0) {
        error(tr("MP3/LAME Error: Programmer mistake: Couldn't initialize lame encoder"));
        return false;
    }

    // Write out the ID3v2 tag
    unsigned char dummybuf[1];
    size_t bufsize = lame_get_id3v2_tag(m_lame_gbf, dummybuf, 1);
    auto buffer = std::make_unique<unsigned char[]>(bufsize);
    lame_get_id3v2_tag(m_lame_gbf, buffer.get(), bufsize);

    if (output->write((const char*)buffer.get(), qint64(bufsize)) < 0)
    {
        error(tr("MP3/LAME Error: Could not write to file: %1").arg(output->errorString()));
        return false;
    }

    return true;
}

LameEncoderStream::~LameEncoderStream()
{
    close();
    lame_close(m_lame_gbf);
}

void LameEncoderStream::close()
{
    if (!m_device)
        return;

    unsigned char lastBits[7200];

    int bytesEncoded = lame_encode_flush_nogap(m_lame_gbf, lastBits, sizeof(lastBits));

    if (bytesEncoded < 0) {
        error(tr("MP3/LAME Error: Probably a programmer mistake. Hint: %1").arg(bytesEncoded));
    } else {
        auto bytesWritten = m_device->write((const char*)lastBits, bytesEncoded);
        if (bytesWritten != bytesEncoded) {
            error(tr("MP3 Error: The device didn't feel like writing all of our data. Sorry."));
        }
    }

    m_device = nullptr;
}

qint64 LameEncoderStream::writeAudio(float *buffer, qint64 numSamples)
{
    unsigned char outBuffer[(numSamples/48000 + 1)*lame_get_brate(m_lame_gbf) + 7200];

    int bytesEncoded = lame_encode_buffer_interleaved_ieee_float(m_lame_gbf, buffer,
        (int)numSamples, outBuffer, sizeof(outBuffer));

    if (bytesEncoded < 0) {
        error(tr("MP3/LAME Error: Probably a programmer mistake. Hint: %1").arg(bytesEncoded));

        close();
        return -1;
    } else {
        auto bytesWritten = m_device->write((const char*)outBuffer, bytesEncoded);
        if (bytesWritten != bytesEncoded) {
            error(tr("MP3 Error: The device didn't feel like writing all of our data. Aborting."));

            close();
            return -1;
        }

        return bytesWritten;
    }
}

} // namespace Recording
