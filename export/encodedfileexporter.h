#ifndef EXPORT_ENCODEDFILEEXPORTER_H
#define EXPORT_ENCODEDFILEEXPORTER_H

#include <QObject>
#include <vector>

#include "error/provider.h"

namespace Recording {
class TrackController;
class TrackDataAccessor;
}

class QIODevice;

namespace Export {

/*!
 * \brief Base class for file-based exporters.
 *
 *        Create in the gui thread, move to separate thread, wire up via signals and go!
 */
class EncodedFileExporter : public QObject
{
    Q_OBJECT
public:
    explicit EncodedFileExporter(QObject *parent = 0);
    virtual ~EncodedFileExporter();

    const Error::Provider *errorProvider() const {
        return m_errorProvider;
    }

    void run(Recording::TrackDataAccessor *accessor, QIODevice *outputFile);

    virtual QString fileExtension() const = 0;

public slots:
    void abort();

signals:
    void samplesProcessed(uint64_t numSamples);

private:
    bool    m_aborted = false;

protected:
    Error::Provider *m_errorProvider;

    /*!
     * \brief beginTrack    Starts encoding of a new track.
     * \param output        An already opened device where subsequent encoded data should be written to
     * \param trackLength   Length of the track in samples
     * \param name          User-Defined track name (for metadata, may be ignored)
     * \param index         Track number, beginning with 0 for the first track (for metadata, may be ignored)
     * \return              Whether the start of the encoding process succeeded. False if errors occured.
     *
     * Errors should be described using the error provider member. If this method fails, neither encodeData
     * nor finishTrack will be called, and the callee is responsible for cleaning up its internal data structures.
     * The given device will be closed by the caller.
     */
    virtual bool beginTrack(QIODevice *output, uint64_t trackLength, const QString& name, int trackIndex) = 0;

    /*!
     * \brief encodeData    Supplies pcm samples to encode.
     * \param buffer        The raw pcm data.
     * \param numSamples    The number of samples.
     * \return              Whether the sample could be processed without errors.
     *
     * The callee is required to consume all data, and may return false if a fatal error occured.
     * Errors may be reported using the error provider. If this function returns false, neither
     * encodeData() nor finishTrack() will be called anymore, and the callee is responsible for cleaning up
     * internal data structures. The IO device will be closed by the caller.
     */
    virtual bool encodeData(const float *buffer, uint64_t numSamples) = 0;

    /*!
     * \brief finishTrack   Finished the encoding process
     * \return              Whether the encoding has been finished sucessfully
     *
     * This function is responsible for cleaning up all internal data structures and writing all remaining
     * data to the IO device given in beginTrack(). The device will be deleted by the caller afterwards.
     */
    virtual bool finishTrack() = 0;

};

} // namespace Export

#endif // EXPORT_ENCODEDFILEEXPORTER_H
