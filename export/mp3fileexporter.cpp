#include "mp3fileexporter.h"

#include <QIODevice>
#include <QDebug>
#include <memory>

namespace Export {

Mp3FileExporter::Mp3FileExporter(const QString& artist, const QString& album, int brate, QObject *parent) :
    EncodedFileExporter(parent),
    m_lame_gbf(lame_init())
{
    lame_set_quality(m_lame_gbf, 5);
    lame_set_mode(m_lame_gbf, JOINT_STEREO);
    lame_set_brate(m_lame_gbf, brate);

    // ID3 tags are barely documented, but luckily we can read the lame(1) source code...
    id3tag_init(m_lame_gbf);
    id3tag_v2_only(m_lame_gbf);

    if (artist.length()) {
        QByteArray artist_u8 = artist.toUtf8();
        id3tag_set_artist(m_lame_gbf, artist_u8.constData());
    }
    if (album.length()) {
        QByteArray album_u8 = album.toUtf8();
        id3tag_set_album(m_lame_gbf, album_u8.constData());
    }
}

Mp3FileExporter::~Mp3FileExporter()
{
    lame_close(m_lame_gbf);
}

} // namespace Export


QString Export::Mp3FileExporter::fileExtension() const
{
    return "mp3";
}

bool Export::Mp3FileExporter::beginTrack(QIODevice *output, uint64_t trackLength, const QString& trackName, int trackIndex)
{
    if (trackLength >= std::numeric_limits<unsigned long>::max()) {
        m_errorProvider->setError(Error::Provider::ErrorType::Error,
                                  tr("MP3/LAME Error"),
                                  tr("Track is too big for the MP3 encoder"));
        return false;
    }

    lame_set_num_samples(m_lame_gbf, (unsigned long)trackLength);

    // set more ID3v2
    if (trackName.length()) {
        QByteArray trackName_u8 = trackName.toUtf8();
        id3tag_set_title(m_lame_gbf, trackName_u8.constData());
    }
    QByteArray trackNo = QString("%1").arg(trackIndex+1).toUtf8();
    id3tag_set_track(m_lame_gbf, trackNo.constData());

    if (lame_init_params(m_lame_gbf) < 0) {
        m_errorProvider->setError(Error::Provider::ErrorType::Error,
                                  tr("MP3/LAME Error"),
                                  tr("Programmer mistake: Couldn't initialize encode"));

        return false;
    }

    // Write out the ID3v2 tag
    unsigned char dummybuf[1];
    size_t bufsize = lame_get_id3v2_tag(m_lame_gbf, dummybuf, 1);
    std::unique_ptr<unsigned char> buffer(new unsigned char[bufsize]);
    lame_get_id3v2_tag(m_lame_gbf, buffer.get(), bufsize);

    qint64 id3_written = output->write((const char*)buffer.get(), (qint64)bufsize);
    if (id3_written != (qint64)bufsize) {
        m_errorProvider->setError(Error::Provider::ErrorType::Error,
                                  tr("MP3 Error"),
                                  tr("The device didn't feel like writing all of our data. Aborting."));

        return false;
    }

    m_device = output;

    return true;
}

bool Export::Mp3FileExporter::encodeData(const char *buffer, uint64_t numSamples)
{
    unsigned char outBuffer[(numSamples/44100 + 1)*lame_get_brate(m_lame_gbf) + 7200];

    int bytesEncoded = lame_encode_buffer_interleaved(m_lame_gbf, (short*)buffer, (int)numSamples, outBuffer, sizeof(outBuffer));

    if (bytesEncoded < 0) {
        m_errorProvider->setError(Error::Provider::ErrorType::Error,
                                  tr("MP3/LAME Error"),
                                  tr("Probably a programmer mistake. Hint: %1").arg(bytesEncoded));

        finishTrack();
        return false;
    } else {
        auto bytesWritten = m_device->write((const char*)outBuffer, bytesEncoded);
        if (bytesWritten != bytesEncoded) {
            m_errorProvider->setError(Error::Provider::ErrorType::Error,
                                      tr("MP3 Error"),
                                      tr("The device didn't feel like writing all of our data. Aborting."));

            finishTrack();
            return false;
        }

        return true;
    }
}

bool Export::Mp3FileExporter::finishTrack()
{
    unsigned char lastBits[7200];

    int bytesEncoded = lame_encode_flush_nogap(m_lame_gbf, lastBits, sizeof(lastBits));

    if (bytesEncoded < 0) {
        m_errorProvider->setError(Error::Provider::ErrorType::Error,
                                  tr("MP3/LAME Error"),
                                  tr("Probably a programmer mistake. Hint: %1").arg(bytesEncoded));

        return false;
    } else {
        auto bytesWritten = m_device->write((const char*)lastBits, bytesEncoded);
        if (bytesWritten != bytesEncoded) {
            m_errorProvider->setError(Error::Provider::ErrorType::Error,
                                      tr("MP3 Error"),
                                      tr("The device didn't feel like writing all of our data. Aborting."));

            return false;
        }

        return true;
    }
}
