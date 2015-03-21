#ifndef TRACKDATAACCESSOR_H
#define TRACKDATAACCESSOR_H

#include <QObject>
#include <QString>
#include <QFile>
#include <QDateTime>
#include <map>

#include "error/provider.h"

/*!
 * \brief The TrackDataAccessor class maintains access to track data
 *
 * The data for one audio track may be shattered over multiple files,
 * and may even be incomplete or inaccessible. The TrackDataAccessor
 * allows to access a continguos stream of samples.
 *
 * Errors are not reported, if data can't be read, silence will be inserted.
 * Error conditions are only available via the errorProvider() object,
 * and are supposed to be displayed directly to the end user.
 */

namespace Recording {

class TrackDataAccessor : public QObject
{
    Q_OBJECT
public:
    explicit TrackDataAccessor(const std::map<uint64_t, QString> &dataFiles,
                               uint64_t startSample,
                               uint64_t length,
                               const QString &name,
                               const QDateTime &timestamp,
                               int trackIndex,
                               QObject *parent = 0);

    const Error::Provider *errorProvider() const {
        return &m_errorProvider;
    }

    uint64_t pos() const {
        return m_currentPos;
    }

    uint64_t length() const {
        return m_length;
    }

    QString name() const {
        return m_name;
    }

    int index() const {
        return m_trackIndex;
    }

    QDateTime timestamp() const {
        return m_timestamp;
    }

    void seek(uint64_t samples_since_track_begin) {
        m_currentPos = qMin(samples_since_track_begin, m_length);
    }

    bool eof() const {
        return m_length == m_currentPos;
    }

    /*!
     * \brief readData  Reads up to n_samples of data into the given buffer
     * \param buffer    Buffer to write the samples into
     * \param n_samples Maximum number of samples to read
     * \return          The number of samples that has been written into the buffer
     *
     * The number of samples that has been written into the buffer can be smaller than
     * the number of samples requested, but will never be zero until you hit the end of
     * the track. If samples cannot be read, silence will be inserted and a human-readable
     * error message will be posted via the error provider.
     */
    uint64_t readData(char *buffer, uint64_t n_samples);

    /*!
     * \brief readDataGuaranteed  Fills the given buffer with the given number of samples
     * \param buffer              Buffer to write the samples into
     * \param n_samples           The number of samples that will be written
     *
     * In contrast to readData, this function guarantees to fill the buffer with the requested
     * number of samples, even if the end of the track is reached or errors occur.
     */
    void readDataGuaranteed(char *buffer, uint64_t n_samples);

    /*!
     * \brief moveToThread Moves this object to the passed thread
     *
     * Helper function for the exporters which need the accessor to reside in their own thread,
     * but can't move the object prior to entering their worker thread.
     */
    Q_INVOKABLE void moveMeToThread(QThread *t);

private:
    Error::Provider m_errorProvider;

    std::map<uint64_t, QFile> m_trackDataFiles;
    uint64_t                  m_currentPos   = 0;
    uint64_t                  m_startSample  = 0;
    uint64_t                  m_length       = 0;
    QString                   m_name;
    int                       m_trackIndex   = 0;
    QDateTime                 m_timestamp;

    /*!
     * \brief Finds the first file belonging to the track at the given position
     * \param currentPos            The number of samples since the beginning of recording
     * \param file                  Return location for the QFile, if any
     * \param nSamplesFromThisFile  Return location for the number of samples that can be read from the
     *                              found file. If this is a value < UINT64_MAX, it means that at most
     *                              *nSamplesFromThisFile samples may be read from the returned file, and
     *                              this function needs to be called again after that to retrieve the next
     *                              file. If *nSamplesFromThisFile is UINT64_MAX, it means that there is no
     *                              "later" file and an indeterminate number of samples may be read from the file.
     * \return                      True, if a file was successfully opened that can be read, false otherwise.
     */
    bool findOpenAndSeekFileForTrack(uint64_t currentPos, QFile **file, uint64_t *nSamplesFromThisFile);
};

} // namespace Recording

#endif // TRACKDATAACCESSOR_H
