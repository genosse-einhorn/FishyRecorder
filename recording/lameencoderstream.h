#ifndef LAMEENCODERSTREAM_H
#define LAMEENCODERSTREAM_H

#include <QObject>

#include <lame/lame.h>

class QIODevice;

namespace Recording {

class LameEncoderStream: public QObject
{
    Q_OBJECT
public:
    explicit LameEncoderStream(QObject *parent = nullptr);
    ~LameEncoderStream();

    void close();

signals:
    void error(const QString &message);

public slots:
    bool init(const QString& artist, const QString &track, int brate, QIODevice *output);

    qint64 writeAudio(float *samples, qint64 count);

private:
    lame_global_flags *m_lame_gbf;
    QIODevice         *m_device;
};

} // namespace Recording

#endif // LAMEENCODERSTREAM_H
