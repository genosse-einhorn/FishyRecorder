#include "trackdataaccessor.h"

#include <QDebug>

namespace Recording {

TrackDataAccessor::TrackDataAccessor(const std::map<uint64_t, QString> &dataFiles,
                                     uint64_t startSample,
                                     uint64_t length,
                                     const QString &name,
                                     const QDateTime &timestamp,
                                     int trackIndex,
                                     QObject *parent) :
    QObject(parent),
    m_startSample(startSample),
    m_length(length),
    m_name(name),
    m_trackIndex(trackIndex),
    m_timestamp(timestamp)
{
    for (auto const& i : dataFiles) {
        // Stuffing a QFile into a STL container is hard. C++11 template hackery saves the day.
        // At least QFile follows RAII principles and closes itself once it's deleted
        m_trackDataFiles.emplace(std::piecewise_construct, std::forward_as_tuple(i.first), std::forward_as_tuple(i.second));
    }
}

uint64_t TrackDataAccessor::readData(float *buffer, uint64_t nSamples)
{
    // first, find the file to begin with
    uint64_t currentTotalPos = m_startSample + m_currentPos;

    QFile *file;
    uint64_t samplesInFile;

    if (findOpenAndSeekFileForTrack(currentTotalPos, &file, &samplesInFile)) {
        uint64_t samplesToRead = qMin(nSamples, samplesInFile);

        int64_t bytesRead = file->read((char*)buffer, samplesToRead*sizeof(float)*2);

        // The debug code will stay in case we inadvertently break it again
        /*qDebug() << "Reading" << nSamples << "samples at sample position" << currentTotalPos;


        qDebug() << QString("At byte offset 0x%1, first sample is %2 %3 %4 %5").arg(currentTotalPos*4, 0, 16)
                                                                               .arg((int)(uint8_t)buffer[0], 0, 16)
                                                                               .arg((int)(uint8_t)buffer[1], 0, 16)
                                                                               .arg((int)(uint8_t)buffer[2], 0, 16)
                                                                               .arg((int)(uint8_t)buffer[3], 0, 16);
        */

        if (bytesRead < 0) {
            m_errorProvider.setError(Error::Provider::ErrorType::Error,
                                     tr("I/O error"),
                                     tr("While reading from `%1': %2").arg(file->fileName()).arg(file->errorString()));

            std::fill_n(buffer, samplesToRead*2, 0.0f);
        } else if ((uint64_t)bytesRead/sizeof(float) < samplesToRead) {
            qDebug() << "Tried to read" << samplesToRead << "samples =" << samplesToRead*4 << "bytes";
            qDebug() << "Got" << bytesRead << "bytes =" << bytesRead/4 << "samples";

            m_errorProvider.setError(Error::Provider::ErrorType::Error,
                                     tr("I/O error"),
                                     tr("There is data missing in `%1', or some internal data structure has been corrupted.").arg(file->fileName()));

            std::fill_n(buffer + (bytesRead/sizeof(float)), 2*samplesToRead - bytesRead/sizeof(float), 0.0f);
        }
    } else {
        uint64_t samplesToFill = qMin(nSamples, samplesInFile);

        std::fill_n(buffer, samplesToFill*2, 0.0f);
    }

    uint64_t samples_filled = qMin(samplesInFile, nSamples);
    m_currentPos += samples_filled;
    return samples_filled;
}

void TrackDataAccessor::readDataGuaranteed(float *buffer, uint64_t n_samples)
{
    while (n_samples > 0) {
        uint64_t samplesRead = readData(buffer, n_samples);

        n_samples -= samplesRead;
        buffer    += samplesRead*2;

        if (samplesRead == 0) //eof
            break;
    }

    if (n_samples) {
        std::fill_n(buffer, n_samples*2, 0.0f);
    }
}

void TrackDataAccessor::moveMeToThread(QThread *t)
{
    this->setParent(nullptr);
    this->moveToThread(t);
}

bool TrackDataAccessor::findOpenAndSeekFileForTrack(uint64_t currentPos, QFile **file, uint64_t *nSamplesFromThisFile)
{
    QFile *fileBefore;
    uint64_t fileBeforePos;
    uint64_t fileAfterPos;
    bool foundFileBefore = false;
    bool foundFileAfter  = false;

    for (auto& i : m_trackDataFiles) {
        if (i.first <= currentPos) {
            fileBefore = &i.second;
            fileBeforePos = i.first;
            foundFileBefore = true;
        }
        if (i.first > currentPos) {
            fileAfterPos = i.first;
            foundFileAfter = true;
        }
    }

    // we can do the sample calculation right away
    if (foundFileAfter)
        *nSamplesFromThisFile = qMin(fileAfterPos, m_startSample + m_length) - currentPos;
    else
        *nSamplesFromThisFile = (m_startSample + m_length) - currentPos;

    // if we already failed to open the file, don't try again
    if (!foundFileBefore || fileBefore->error() == QFileDevice::OpenError)
        return false;

    if (!fileBefore->isOpen()) {
        fileBefore->open(QIODevice::ReadOnly);

        if (fileBefore->error()) {
            m_errorProvider.setError(Error::Provider::ErrorType::Error,
                                     tr("I/O error"),
                                     tr("Couldn't access track data: Couldn't open file `%1': %2").arg(fileBefore->fileName()).arg(fileBefore->errorString()));

            return false;
        }
    }

    if (!fileBefore->seek(((qint64)(currentPos - fileBeforePos))*sizeof(float)*2)) {
        m_errorProvider.setError(Error::Provider::ErrorType::Error,
                                 tr("I/O error"),
                                 tr("Could not seek to sample %1 in file `%2': %3").arg(currentPos - fileBeforePos)
                                                                                   .arg(fileBefore->fileName())
                                                                                   .arg(fileBefore->errorString()));

        return false;
    }

    // ok, we have our file
    *file = fileBefore;

    return true;
}

} // namespace Recording
