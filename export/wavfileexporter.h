#ifndef EXPORT_WAVFILEEXPORTER_H
#define EXPORT_WAVFILEEXPORTER_H

#include "encodedfileexporter.h"

namespace Recoding {
    class TrackController;
}

namespace Export {

class WavFileExporter : public EncodedFileExporter
{
    Q_OBJECT
public:
    explicit WavFileExporter(QObject *parent = 0);

    static EncodedFileExporter* create() { return new WavFileExporter(); }

signals:

public slots:

private:
    QIODevice *m_device          = nullptr;
    uint64_t   m_samplesPromised = 0;
    uint64_t   m_samplesWritten  = 0;


    // EncodedFileExporter interface
protected:
    virtual QString fileExtension() const override;
    virtual bool beginTrack(QIODevice *output, uint64_t trackLength, const QString &name, int trackIndex) override;
    virtual bool encodeData(const float *buffer, uint64_t numSamples) override;
    virtual bool finishTrack();
};

} // namespace Export

#endif // EXPORT_WAVFILEEXPORTER_H
