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
    explicit WavFileExporter(const Recording::TrackController *controller, const QString& ouputDir, QObject *parent = 0);

signals:

public slots:

private:
    QIODevice *m_device          = nullptr;
    uint64_t   m_samplesPromised = 0;
    uint64_t   m_samplesWritten  = 0;


    // EncodedFileExporter interface
protected:
    virtual QString fileExtension();
    virtual bool beginTrack(QIODevice *output, uint64_t trackLength, const QString &name);
    virtual bool encodeData(const char *buffer, uint64_t numSamples);
    virtual bool finishTrack();
};

} // namespace Export

#endif // EXPORT_WAVFILEEXPORTER_H
