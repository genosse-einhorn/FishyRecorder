#ifndef EXPORT_COORDINATOR_H
#define EXPORT_COORDINATOR_H

#include <QObject>
#include <QFutureWatcher>
#include <functional>
#include <map>
#include <atomic>
#include <error/provider.h>

class QIODevice;

namespace Recording {
class TrackController;
class TrackDataAccessor;
}

namespace Export {

class EncodedFileExporter;

class Coordinator : public QObject
{
    Q_OBJECT
public:
    explicit Coordinator(const Recording::TrackController *controller,
                         const QString& outputDir,
                         std::function<EncodedFileExporter*()> encoderFactory,
                         QObject *parent = 0);
    ~Coordinator();

signals:
    //! Misnamed: goes from 0.0d to 1.0d
    void percentage(double fraction);
    void message(Error::Provider::ErrorType severity, const QString& title, const QString& message);
    void finished(bool successful, const QString& message);

    void privSendAbort();

public slots:
    void start();
    void abort();

private slots:
    void deliverMessage(Error::Provider::ErrorType severity, const QString& title, const QString& message);
    void samplesProcessed(uint64_t numSamples);
    void mapFinished();
    void mapAborted();

private:
    void encodeTrack(std::pair<const unsigned, Recording::TrackDataAccessor *> &accessor);

    const Recording::TrackController *m_trackController;
    QString                           m_outputDir;
    int                               m_trackNoChars;

    std::function<EncodedFileExporter*()>             m_encoderFactory;
    QFutureWatcher<void>                             *m_doneWatcher = nullptr;
    std::map<unsigned, Recording::TrackDataAccessor*> m_accessors;

    bool m_errorSeen = false; // set to true whenever we deliver an error
    std::atomic<bool> m_aborted { false }; // might be accessed by workers

    uint64_t m_totalSamples     = 0;
    uint64_t m_processedSamples = 0;
};

} // namespace Export

#endif // EXPORT_COORDINATOR_H
