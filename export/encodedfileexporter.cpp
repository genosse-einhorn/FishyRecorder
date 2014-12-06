#include "encodedfileexporter.h"

#include "recording/trackdataaccessor.h"
#include "recording/trackcontroller.h"

#include <QCoreApplication>
#include <algorithm>

namespace Export {

EncodedFileExporter::EncodedFileExporter(const Recording::TrackController *controller, const QString &outputDir, QObject *parent) :
    QObject(parent), m_outputDir(outputDir), m_errorProvider(new Error::Provider(this))
{
    // we create all the accessors in the constructor so we do not depend on the
    // lifetime of the controller and avoid threading issues
    auto trackCount = controller->getTrackCount();
    for (unsigned i = 0; i < trackCount; ++i) {
        auto accessor = controller->accessTrackData(i);
        accessor->setParent(this);
        QObject::connect(accessor->errorProvider(), &Error::Provider::error, m_errorProvider, &Error::Provider::setError);
        m_trackDataAccessors.push_back(accessor);
    }
}

static const unsigned int BUFFER_SIZE = 10240;

void EncodedFileExporter::start()
{
    m_isAborted = false;

    int max_num_length = QString("%1").arg((unsigned)m_trackDataAccessors.size()).length();

    uint64_t total_sample_length = std::accumulate(m_trackDataAccessors.cbegin(), m_trackDataAccessors.cend(), 0,
                                                   [](uint64_t start, const Recording::TrackDataAccessor* accessor) { return start + accessor->getLength(); });
    uint64_t total_samples_processed = 0;

    for (decltype(m_trackDataAccessors)::size_type i = 0; i < m_trackDataAccessors.size(); ++i) {
        auto accessor = m_trackDataAccessors[i];

        // open the file
        //FIXME: This is racy, unsafe and probably incorrect
        QString fileName = QString("%1/%2.%3").arg(m_outputDir).arg((unsigned)i+1, max_num_length, 10, QChar('0')).arg(fileExtension());
        for (size_t j = 1; QFile::exists(fileName); ++j) {
            fileName = QString("%1/%2 (%4).%3").arg(m_outputDir).arg((unsigned)i+1, max_num_length, 10, QChar('0')).arg(fileExtension()).arg(j);
        }

        QFile outputFile(fileName);

        if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            m_errorProvider->setError(Error::Provider::ErrorType::Error,
                                      tr("IO Error"),
                                      tr("Could not open file `%1': %2").arg(fileName).arg(outputFile.errorString()));
            continue; // try next track
        } else {
            m_errorProvider->setError(Error::Provider::ErrorType::Notice,
                                      tr("Information"),
                                      tr("Beginning to write track %1").arg(i));
        }




        // go through the raw samples
        char buffer[BUFFER_SIZE];
        uint64_t samples_processed = 0;

        // tell the slave to begin working
        if (!beginTrack(&outputFile, accessor->getLength(), "FIXME"))
            goto finished;

        while (samples_processed < accessor->getLength()) {
            uint64_t samples_this_time = accessor->readData(buffer, BUFFER_SIZE/4);

            samples_processed += samples_this_time;
            total_samples_processed += samples_this_time;

            // safety measure, should not be necessary
            if (samples_this_time == 0)
                break;

            if (!encodeData(buffer, samples_this_time))
                goto finished;

            emit percentage(total_samples_processed / total_sample_length);

            // we do not want to stall our event loop
            QCoreApplication::processEvents();

            if (m_isAborted)
                break;
        }

        finishTrack();

        m_errorProvider->setError(Error::Provider::ErrorType::Notice,
                                  tr("Information"),
                                  tr("Finished writing track %1").arg(i));

    finished:
        outputFile.close();

        if (m_isAborted) {
            m_errorProvider->setError(Error::Provider::ErrorType::Warning,
                                      tr("Aborted by user"),
                                      tr("You aborted the encoding process while writing track %1").arg(i));
            break;
        }
    }

    emit finished();
}

void EncodedFileExporter::abort()
{
    m_isAborted = true;
}

} // namespace Export
