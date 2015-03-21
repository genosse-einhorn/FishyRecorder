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
    Mp3FileExporter(const QString& artist, const QString& album, int brate, QObject *parent = 0);
    ~Mp3FileExporter();

    // EncodedFileExporter interface
protected:
    virtual QString fileExtension() const override;
    virtual bool beginTrack(QIODevice *output, uint64_t trackLength, const QString& trackName, int trackIndex) override;
    virtual bool encodeData(const char *buffer, uint64_t numSamples) override;
    virtual bool finishTrack() override;

private:
    lame_global_flags *m_lame_gbf;
    QIODevice         *m_device;
};

} // namespace Export

#endif // EXPORT_MP3FILEEXPORTER_H
