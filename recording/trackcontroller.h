#ifndef TRACKRECORDINGCONTROLLER_H
#define TRACKRECORDINGCONTROLLER_H

#include <QObject>
#include <QDateTime>
#include <cstdint>

namespace Config {
    class Database;
}

namespace Recording {

class TrackDataAccessor;

/**
 * @brief The TrackController class manages everything related to tracks
 *
 * The track controller
 * - keeps a list of recorded tracks
 * - keeps a list of files containing the audio data
 * - syncs itself with the database
 * - modifies the list of tracks whenever that is requested
 */
class TrackController : public QObject
{
    Q_OBJECT
public:
    explicit TrackController(Config::Database *db, QObject *parent = 0);

    // REGION getters
    int       trackCount() const;
    QString   trackName(int trackIndex) const;
    uint64_t  trackStart(int trackIndex) const;
    uint64_t  trackLength(int trackIndex) const;
    QDateTime trackTimestamp(int trackIndex) const;

    //! Whether the given track is in the process of being recorded
    bool      trackBeingRecorded(int trackIndex) const;

    TrackDataAccessor *accessTrackData(int trackId) const;

    int trackIdFromStart(uint64_t trackStart);

signals:
    void currentTrackTimeChanged(uint64_t n_samples);
    void beforeTrackAdded(int newTrackIndex);
    void trackAdded(int trackIndex);
    void beforeTrackRemoved(int oldTrackIndex);
    void trackRemoved(int trackIndex);
    void trackChanged(int trackIndex);

public slots:
    void onRecordingStateChanged(bool recording, uint64_t n_samples);
    void onRecordingFileChanged(const QString& new_file_path, uint64_t starting_sample_count);
    void onTimeUpdate(uint64_t recorded_samples);
    void startNewTrack(uint64_t sample_count);
    void setTrackName(int trackIndex, const QString &name);
    void splitTrack(int trackIndex, uint64_t after_n_samples);
    void deleteTrack(int trackIndex);

private:
    struct Track {
        uint64_t  start;
        uint64_t  length;
        QString   name;
        QDateTime timestamp;
    };

    std::map<uint64_t,QString> m_rawDataFiles;
    std::vector<Track>         m_tracks;

    bool m_currentlyRecording = false;

    Config::Database *m_database;

    void insertTrack(int beforeTrackIndex, uint64_t start, uint64_t length, const QString &name, const QDateTime &timestamp);
    void updateTrackLength(int trackIndex, uint64_t newLength, bool syncToDb = true);
    bool trackIndexValid(int trackIndex) const;
};

} // namespace Recording

#endif // TRACKRECORDINGCONTROLLER_H
