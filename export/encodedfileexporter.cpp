#include "encodedfileexporter.h"

#include "recording/trackdataaccessor.h"
#include "recording/trackcontroller.h"

#include "error/simulation.h"

#include <QThread>
#include <QCoreApplication>
#include <algorithm>

namespace Export {

EncodedFileExporter::EncodedFileExporter(QObject *parent) :
    QObject(parent), m_errorProvider(new Error::Provider(this))
{
}

EncodedFileExporter::~EncodedFileExporter()
{

}

static const unsigned int BUFFER_SIZE = 10240;

void EncodedFileExporter::run(Recording::TrackDataAccessor *accessor, QIODevice *outputFile)
{
    // go through the raw samples
    float buffer[BUFFER_SIZE];
    uint64_t samples_processed = 0;

    // tell the slave to begin working
    if (!beginTrack(outputFile, accessor->length(), accessor->name(), accessor->index()))
        return;

    while (samples_processed < accessor->length()) {
        if (Error::Simulation::SIMULATE("slowdownExport"))
            QThread::msleep(100);

        // our event loop is solely responsible for delivering abort messages to us
        QCoreApplication::processEvents();

        if (m_aborted)
            break;

        uint64_t samples_this_time = accessor->readData(buffer, BUFFER_SIZE/2);
        samples_processed += samples_this_time;

        // safety measure, should not be necessary
        if (samples_this_time == 0)
            break;

        if (!encodeData(buffer, samples_this_time))
            return;

        emit samplesProcessed(samples_this_time);
    }

    finishTrack();
}

void EncodedFileExporter::abort()
{
    m_aborted = true;
}

} // namespace Export
