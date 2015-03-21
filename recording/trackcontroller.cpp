#include "recording/trackcontroller.h"
#include "recording/trackdataaccessor.h"
#include "config/database.h"

#include <QDebug>

Recording::TrackController::TrackController(Config::Database *db, QObject *parent) :
    QObject(parent), m_database(db)
{
    // initialize tracks and files based on the database
    for (const auto& track : m_database->readAllTracks()) {
        Track t { track.start, track.length, track.name, track.timestamp };
        m_tracks.push_back(t);
    }
    for (const auto& file : m_database->readAllFiles()) {
        m_rawDataFiles[file.first] = file.second;
    }
}

int Recording::TrackController::trackCount() const
{
    return (int)m_tracks.size();
}

QString Recording::TrackController::trackName(int trackIndex) const
{
    if (!trackIndexValid(trackIndex))
        return QString();

    return m_tracks[trackIndex].name;
}

uint64_t Recording::TrackController::trackStart(int trackIndex) const
{
    if (!trackIndexValid(trackIndex))
        return 0;

    return m_tracks[trackIndex].start;
}

uint64_t Recording::TrackController::trackLength(int trackIndex) const
{
    if (!trackIndexValid(trackIndex))
        return 0;

    return m_tracks[trackIndex].length;
}

QDateTime Recording::TrackController::trackTimestamp(int trackIndex) const
{
    if (!trackIndexValid(trackIndex))
        return QDateTime();

    return m_tracks[trackIndex].timestamp;
}

bool Recording::TrackController::trackBeingRecorded(int trackIndex) const
{
    return (trackIndex == ((int)m_tracks.size()-1)) && m_currentlyRecording;
}

void Recording::TrackController::splitTrack(int trackIndex, uint64_t after_n_samples)
{
    if (!trackIndexValid(trackIndex))
       return;

    Track originalTrack = m_tracks[trackIndex];

    if (after_n_samples >= originalTrack.length)
        return;


    this->updateTrackLength(trackIndex, after_n_samples);

    this->insertTrack(trackIndex+1,
                      originalTrack.start + after_n_samples,
                      originalTrack.length - after_n_samples,
                      QString(),
                      originalTrack.timestamp.addMSecs(after_n_samples * 1000 / 44100)); //FIXME: check overflow
}

void Recording::TrackController::deleteTrack(int trackIndex)
{
    if (!trackIndexValid(trackIndex))
        return;

    emit beforeTrackRemoved(trackIndex);

    m_database->removeTrack(m_tracks[trackIndex].start);
    m_tracks.erase(m_tracks.begin() + trackIndex);

    emit trackRemoved(trackIndex);
}

void Recording::TrackController::insertTrack(int beforeTrackIndex, uint64_t start, uint64_t length, const QString &name, const QDateTime &timestamp)
{
    emit beforeTrackAdded(beforeTrackIndex + 1);

    Track newTrack { start,
                     length,
                     name,
                     timestamp };
    m_tracks.insert(m_tracks.begin() + beforeTrackIndex, newTrack);
    m_database->insertTrack(newTrack.start, newTrack.length, newTrack.name, newTrack.timestamp);

    emit trackAdded(beforeTrackIndex + 1);
}

void Recording::TrackController::updateTrackLength(int trackIndex, uint64_t newLength, bool syncToDb)
{
    m_tracks[trackIndex].length = newLength;

    if (syncToDb)
        m_database->updateTrackLength(m_tracks[trackIndex].start, m_tracks[trackIndex].length);

    emit trackChanged(trackIndex);
}

bool Recording::TrackController::trackIndexValid(int trackIndex) const
{
    return trackIndex >= 0 && trackIndex < (int)m_tracks.size();
}

Recording::TrackDataAccessor *Recording::TrackController::accessTrackData(int trackId) const
{
    if (!trackIndexValid(trackId))
        return nullptr;

    return new Recording::TrackDataAccessor(m_rawDataFiles, m_tracks[trackId].start, m_tracks[trackId].length);
}

void Recording::TrackController::onRecordingStateChanged(bool recording, uint64_t n_samples)
{
    if (recording == m_currentlyRecording)
        return; //FIXME: should this happen? should we log it?

    m_currentlyRecording = recording;

    qDebug() << "TrackController: registered recording state change, now" << (recording ? "recording" : "stopped") << "at sample" << n_samples;

    if (recording) {
        qDebug() << "Beginning Track " << m_tracks.size();

        this->insertTrack(trackCount(), n_samples, 0, QString(), QDateTime::currentDateTimeUtc());

        emit currentTrackTimeChanged(0);
    } else {
        // finish last track
        this->updateTrackLength(trackCount()-1, n_samples - m_tracks.back().start);

        qDebug() << "Updated length of last track, now " << m_tracks.back().length;
    }
}

void Recording::TrackController::onRecordingFileChanged(const QString &new_file_path, uint64_t starting_sample_count)
{
    m_rawDataFiles[starting_sample_count] = new_file_path;

    m_database->appendFile(starting_sample_count, new_file_path);

    qDebug() << "Got new file: " << new_file_path << " for sample " << starting_sample_count;
}

void Recording::TrackController::onTimeUpdate(uint64_t recorded_samples)
{
    if (m_currentlyRecording) {
        this->updateTrackLength(trackCount()-1, recorded_samples - m_tracks.back().start, false);

        emit currentTrackTimeChanged(m_tracks.back().length);
    }
}

void Recording::TrackController::startNewTrack(uint64_t sample_count)
{
    if (m_currentlyRecording) {
        qDebug() << "Beginning track " << m_tracks.size();

        this->updateTrackLength(trackCount()-1, sample_count - m_tracks.back().start);
        this->insertTrack(trackCount(), sample_count, 0, QString(), QDateTime::currentDateTimeUtc());

        emit currentTrackTimeChanged(0);
    } else {
        qWarning() << "Starting a new track when you're not recording doesn't make any sense "
                   << "and should be disallowed in the user interface. ";
    }
}

void Recording::TrackController::setTrackName(int trackIndex, const QString &name)
{
    if (!trackIndexValid(trackIndex))
        return;

    m_tracks[trackIndex].name = name;
    m_database->updateTrackName(m_tracks[trackIndex].start, m_tracks[trackIndex].name);

    emit trackChanged(trackIndex);
}
