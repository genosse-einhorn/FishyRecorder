#include "mp3fileexporter.h"

#include <QIODevice>

namespace Export {

Mp3FileExporter::Mp3FileExporter(QObject *parent) :
    EncodedFileExporter(parent),
    m_lame_gbf(lame_init())
{
    lame_set_quality(m_lame_gbf, 5);
    lame_set_mode(m_lame_gbf, JOINT_STEREO);
    lame_set_brate(m_lame_gbf, 192);
}

} // namespace Export


QString Export::Mp3FileExporter::fileExtension()
{
    return "mp3";
}

bool Export::Mp3FileExporter::beginTrack(QIODevice *output, uint64_t trackLength)
{
    if (trackLength >= std::numeric_limits<unsigned long>::max()) {
        m_errorProvider->setError(Error::Provider::ErrorType::Error,
                                  tr("MP3/LAME Error"),
                                  tr("Track is too big for the MP3 encoder"));
        return false;
    }

    lame_set_num_samples(m_lame_gbf, (unsigned long)trackLength);

    if (lame_init_params(m_lame_gbf) < 0) {
        m_errorProvider->setError(Error::Provider::ErrorType::Error,
                                  tr("MP3/LAME Error"),
                                  tr("Programmer mistake: Couldn't initialize encode"));

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

    int bytesEncoded = lame_encode_flush(m_lame_gbf, lastBits, sizeof(lastBits));

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
