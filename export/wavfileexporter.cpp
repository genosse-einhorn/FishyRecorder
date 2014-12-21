#include "wavfileexporter.h"
#include "recording/trackcontroller.h"

#include <string.h>
#include <QIODevice>
#include <QDebug>

namespace {
#pragma pack(push,1)

struct RiffHeader {
    char     chunkID[4]; // "RIFF"
    uint32_t chunkSize;  // excluding header
    char     format[4];  // "WAVE"
};

struct FmtHeader {
    char     chunkID[4]; // "fmt "
    uint32_t chunkSize;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
};

struct DataHeader {
    char     chunkID[4]; // "data"
    uint32_t chunkSize;  // data size
};

#pragma pack(pop)
} // namespace

namespace Export {

WavFileExporter::WavFileExporter(QObject *parent) :
    EncodedFileExporter(parent)
{
}

QString WavFileExporter::fileExtension()
{
    return "wav";
}

bool WavFileExporter::beginTrack(QIODevice *output, uint64_t trackLength)
{
    if (trackLength >= std::numeric_limits<uint32_t>::max()/4) {
        m_errorProvider->setError(Error::Provider::ErrorType::Error,
                                  tr("WAV Error"),
                                  tr("Track is too big to fit in a WAV file"));
        return false;
    }

    m_device = output;
    m_samplesPromised = trackLength;

    RiffHeader riff;
    FmtHeader  fmt;
    DataHeader data;

    strncpy(data.chunkID, "data", 4);
    data.chunkSize = (uint32_t)trackLength * 4;

    strncpy(riff.chunkID, "RIFF", 4);
    riff.chunkSize = 4 + sizeof(FmtHeader) + sizeof(DataHeader) + data.chunkSize;
    strncpy(riff.format, "WAVE", 4);

    strncpy(fmt.chunkID,  "fmt ",  4);
    fmt.chunkSize = sizeof(FmtHeader)-8;
    fmt.audioFormat   = 1;
    fmt.numChannels   = 2;
    fmt.sampleRate    = 44100;
    fmt.byteRate      = fmt.sampleRate * fmt.numChannels * sizeof(uint16_t);
    fmt.blockAlign    = fmt.numChannels * sizeof(uint16_t);
    fmt.bitsPerSample = sizeof(uint16_t)*8;

    if (output->write((char *)&riff, sizeof(RiffHeader)) == sizeof(RiffHeader)
            && output->write((char *)&fmt, sizeof(FmtHeader)) == sizeof(FmtHeader)
            && output->write((char *)&data, sizeof(DataHeader)) == sizeof(DataHeader)) {
        return true;
    } else {
        m_errorProvider->setError(Error::Provider::ErrorType::Error,
                                  tr("IO Error"),
                                  output->errorString());
        return false;
    }
}

bool WavFileExporter::encodeData(const char *buffer, uint64_t numSamples)
{
    // The debug code will stay in case we inadvertently break it again
    /*qDebug() << "Encoding " << numSamples << "wave samples at pos" << m_samplesWritten;
    qDebug() << QString("At byte offset 0x%1, first sample is %2 %3 %4 %5").arg(m_samplesWritten*4, 0, 16)
                                                                           .arg((int)(uint8_t)buffer[0], 0, 16)
                                                                           .arg((int)(uint8_t)buffer[1], 0, 16)
                                                                           .arg((int)(uint8_t)buffer[2], 0, 16)
                                                                           .arg((int)(uint8_t)buffer[3], 0, 16);
    */

    m_samplesWritten += numSamples;

    int64_t bytesWritten = m_device->write(buffer, numSamples*4);

    if (bytesWritten < 0) {
        m_errorProvider->setError(Error::Provider::ErrorType::Error,
                                  tr("IO Error"),
                                  m_device->errorString());

        return false;
    } else if ((uint64_t)bytesWritten < numSamples*4) {
        m_errorProvider->setError(Error::Provider::ErrorType::Error,
                                  tr("WAV IO Error"),
                                  tr("FIXME: Device didn't feel like writing all of our data"));

        return false;
    } else {
        return true;
    }
}

bool WavFileExporter::finishTrack()
{
    //TODO: should we fill up with zeroes if samples are missing?
    return true;
}


} // namespace Export


