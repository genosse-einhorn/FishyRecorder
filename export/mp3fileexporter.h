#ifndef EXPORT_MP3FILEEXPORTER_H
#define EXPORT_MP3FILEEXPORTER_H

#include <export/encodedfileexporter.h>
#include <lame/lame.h>

#include <QObject>

namespace Export {

class Mp3FileExporter : public Export::EncodedFileExporter
{
    Q_OBJECT
public:
    Mp3FileExporter(const Recording::TrackController *controller, const QString& outputDir, QObject *parent = 0);

    // EncodedFileExporter interface
protected:
    virtual QString fileExtension();
    virtual bool beginTrack(QIODevice *output, uint64_t trackLength, const QString &name);
    virtual bool encodeData(const char *buffer, uint64_t numSamples);
    virtual bool finishTrack();

private:
    lame_global_flags *m_lame_gbf;
    QIODevice         *m_device;
};

} // namespace Export

#endif // EXPORT_MP3FILEEXPORTER_H
