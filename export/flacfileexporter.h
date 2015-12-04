#ifndef EXPORT_FLACFILEEXPORTER_H
#define EXPORT_FLACFILEEXPORTER_H

#include "encodedfileexporter.h"
#include "FLAC/stream_encoder.h"

namespace Export {

class FlacFileExporter : public Export::EncodedFileExporter
{
    Q_OBJECT
public:
    FlacFileExporter(QObject *parent = 0);
    ~FlacFileExporter();

    // EncodedFileExporter interface
protected:
    virtual QString fileExtension() const override;
    virtual bool beginTrack(QIODevice *output, uint64_t trackLength, const QString& trackName, int trackIndex) override;
    virtual bool encodeData(const float *buffer, uint64_t numSamples) override;
    virtual bool finishTrack() override;

private:
    FLAC__StreamEncoder *m_encoder;
    QIODevice           *m_device;

    // Dithering state
    int   r1, r2;                //rectangular-PDF random numbers
    float s1, s2;                //error feedback buffers
    float s  = 0.5f;              //set to 0.0f for no noise shaping
    float w  = std::pow(2.0, 16-1);   //word length (usually bits=16)
    float wi = 1.0f / w;
    float d  = wi / RAND_MAX;     //dither amplitude (2 lsb)
    float o  = wi * 0.5f;         //remove dc offset

    int32_t doDither(float in);
};

} // namespace Export

#endif // EXPORT_FLACFILEEXPORTER_H
