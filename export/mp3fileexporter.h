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
    Mp3FileExporter(QObject *parent = 0);

    static EncodedFileExporter* create() { return new Mp3FileExporter(); }

    // EncodedFileExporter interface
protected:
    virtual QString fileExtension();
    virtual bool beginTrack(QIODevice *output, uint64_t trackLength);
    virtual bool encodeData(const char *buffer, uint64_t numSamples);
    virtual bool finishTrack();

private:
    lame_global_flags *m_lame_gbf;
    QIODevice         *m_device;
};

} // namespace Export

#endif // EXPORT_MP3FILEEXPORTER_H
