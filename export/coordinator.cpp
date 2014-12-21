#include "coordinator.h"

#include <QtConcurrent>

#include "recording/trackcontroller.h"
#include "recording/trackdataaccessor.h"
#include "export/encodedfileexporter.h"

namespace Export {

Coordinator::Coordinator(const Recording::TrackController *controller,
                         const QString &outputDir,
                         std::function<EncodedFileExporter*()> encoderFactory,
                         QObject *parent) :
    QObject(parent),
    m_trackController(controller),
    m_outputDir(outputDir),
    m_encoderFactory(encoderFactory)
{
    for (unsigned i = 0; i < m_trackController->getTrackCount(); ++i) {
        m_accessors[i] = m_trackController->accessTrackData(i);
        m_accessors[i]->setParent(this); // Cleanup in case it isn't processed
        m_totalSamples += m_trackController->getTrackLength(i);
    }

    m_trackNoChars = QString("%1").arg(m_trackController->getTrackCount()-1).size();
}

Coordinator::~Coordinator()
{
    // our parallelized workers have access to private instance data, we need them
    // all to be canceled before the memory is gone
    if (m_doneWatcher) {
        m_doneWatcher->cancel();
        m_doneWatcher->waitForFinished();
    }
}

void Coordinator::start()
{
    using namespace std::placeholders;

    if (m_doneWatcher)
        return;

    m_doneWatcher = new QFutureWatcher<void>(this);

    QObject::connect(m_doneWatcher, &QFutureWatcher<void>::finished, this, &Coordinator::mapFinished);
    QObject::connect(m_doneWatcher, &QFutureWatcher<void>::canceled, this, &Coordinator::mapAborted);

    m_doneWatcher->setFuture(QtConcurrent::map(m_accessors, std::bind(&Coordinator::encodeTrack, this, _1)));
}

void Coordinator::abort()
{
    if (m_doneWatcher)
        m_doneWatcher->cancel();

    emit privSendAbort();
}

void Coordinator::deliverMessage(Error::Provider::ErrorType severity, const QString &title, const QString &message)
{
    emit this->message(severity, title, message);

    if (severity != Error::Provider::ErrorType::Notice && severity != Error::Provider::ErrorType::NoError)
        m_errorSeen = true;
}

void Coordinator::samplesProcessed(uint64_t numSamples)
{
    m_processedSamples += numSamples;

    emit percentage((double)m_processedSamples/m_totalSamples);
}

void Coordinator::mapFinished()
{
    if (m_aborted)
        emit finished(false, tr("Export has been aborted."));
    else if (m_errorSeen)
        emit finished(false, tr("Export finished, errors occured."));
    else
        emit finished(true, tr("Export finished successfully."));
}

void Coordinator::mapAborted()
{
    m_aborted = true;
}

void Coordinator::encodeTrack(std::pair<const unsigned, Recording::TrackDataAccessor*> &accessor)
{
    if (m_aborted)
        return;

    QMetaObject::invokeMethod(accessor.second, "moveMeToThread", Qt::BlockingQueuedConnection, Q_ARG(QThread*, QThread::currentThread()));

    // create encoder
    EncodedFileExporter *encoder = m_encoderFactory();

    QObject::connect(encoder->errorProvider(), &Error::Provider::error, this, &Coordinator::deliverMessage);
    QObject::connect(encoder, &EncodedFileExporter::samplesProcessed, this, &Coordinator::samplesProcessed);
    QObject::connect(accessor.second->errorProvider(), &Error::Provider::error, this, &Coordinator::deliverMessage);
    QObject::connect(this, &Coordinator::privSendAbort, encoder, &EncodedFileExporter::abort);

    // open the file
    //FIXME: This is racy, unsafe and probably incorrect
    QString fileName = QString("%1/%2.%3").arg(m_outputDir).arg(accessor.first+1, m_trackNoChars, 10, QChar('0')).arg(encoder->fileExtension());
    for (size_t j = 1; QFile::exists(fileName); ++j) {
        fileName = QString("%1/%2 (%4).%3").arg(m_outputDir).arg(accessor.first+1, m_trackNoChars, 10, QChar('0')).arg(encoder->fileExtension()).arg(j);
    }

    QFile outputFile(fileName);

    if (!outputFile.open(QFile::WriteOnly | QFile::Truncate)) {
        deliverMessage(Error::Provider::ErrorType::Error, tr("I/O Error"), tr("While opening `%1': %2").arg(fileName).arg(outputFile.errorString()));
    } else {
        deliverMessage(Error::Provider::ErrorType::Notice, tr("Information"), tr("Beginning to write track %1").arg(accessor.first+1));

        encoder->run(accessor.second, &outputFile);

        deliverMessage(Error::Provider::ErrorType::Notice, tr("Information"), tr("Finished writing track %1").arg(accessor.first+1));
    }

    delete encoder;
    delete accessor.second;
    accessor.second = nullptr;
}

} // namespace Export
