#include "recording/trackcontroller.h"
#include "recording/trackdataaccessor.h"
#include "config/database.h"

#include <QDebug>

static QString samples_to_time(uint64_t n_samples)
{
    uint64_t minutes = n_samples/44100/60;
    uint64_t seconds = n_samples/44100 % 60;

    return QString("%1:%2").arg(minutes).arg(seconds, 2, 10, QChar('0'));
}

Recording::TrackController::TrackController(Config::Database *db, QObject *parent) :
    QAbstractTableModel(parent), m_database(db)
{
    // initialize tracks and files based on the database
    for (const auto& track : m_database->readAllTracks()) {
        Track t { track.start, track.length, track.name };
        m_tracks.push_back(t);
    }
    for (const auto& file : m_database->readAllFiles()) {
        m_rawDataFiles[file.first] = file.second;
    }
}

unsigned Recording::TrackController::getTrackCount() const
{
    return (unsigned)m_tracks.size();
}

QString Recording::TrackController::getTrackName(unsigned trackIndex) const
{
    if (trackIndex >= m_tracks.size())
        return QString();

    return m_tracks[trackIndex].name;
}

uint64_t Recording::TrackController::getTrackLength(unsigned trackIndex) const
{
    if (trackIndex >= m_tracks.size())
        return 0;

    return m_tracks[trackIndex].length;
}

void Recording::TrackController::splitTrack(unsigned trackIndex, uint64_t after_n_samples)
{
    if (trackIndex >= m_tracks.size())
       return;

    Track originalTrack = m_tracks[trackIndex];

    if (after_n_samples >= originalTrack.length)
        return;


    m_tracks[trackIndex].length = after_n_samples;
    emit dataChanged(createIndex(trackIndex, COL_LENGTH), createIndex(trackIndex, COL_LENGTH));
    m_database->updateTrackLength(m_tracks[trackIndex].start, m_tracks[trackIndex].length);

    beginInsertRows(QModelIndex(), trackIndex+1, trackIndex+1);

    Track newTrack { originalTrack.start + after_n_samples, originalTrack.length - after_n_samples, QString() };
    m_tracks.insert(m_tracks.begin() + trackIndex + 1, newTrack);
    m_database->insertTrack(newTrack.start, newTrack.length, QString());

    endInsertRows();
}

void Recording::TrackController::deleteTrack(unsigned trackIndex)
{
    if (trackIndex >= m_tracks.size())
        return;

    beginRemoveRows(QModelIndex(), trackIndex, trackIndex);
    m_database->removeTrack(m_tracks[trackIndex].start);
    m_tracks.erase(m_tracks.begin() + trackIndex);
    endRemoveRows();
}

Recording::TrackDataAccessor *Recording::TrackController::accessTrackData(unsigned trackId) const
{
    if (trackId >= m_tracks.size())
        return nullptr;

    return new Recording::TrackDataAccessor(m_rawDataFiles, m_tracks[trackId].start, m_tracks[trackId].length);
}

void Recording::TrackController::onRecordingStateChanged(bool recording, uint64_t n_samples)
{
    if (recording == m_currentlyRecording)
        return; //FIXME: should this happen? should we log it?
    else
        m_currentlyRecording = recording;

    qDebug() << "TrackController: registered recording state change, now " << (recording ? "recording" : "stopped");

    if (recording) {
        qDebug() << "Beginning Track " << m_tracks.size();

        // create new track
        beginInsertRows(QModelIndex(), (int)m_tracks.size(), (int)m_tracks.size());

        Track t { n_samples, 0, QString() };
        m_tracks.push_back(t);

        m_database->insertTrack(n_samples, 0, QString());

        endInsertRows();

        emit currentTrackTimeChanged(0);
    } else {
        // finish last track
        m_tracks.back().length = n_samples - m_tracks.back().start;

        qDebug() << "Updated length of last track, now " << m_tracks.back().length;

        // when we stop recording, the flags of the whole last row changed, so
        // we will emit the event for the whole row
        auto index_a = createIndex(m_tracks.size() - 1, COL__FIRST);
        auto index_b = createIndex(m_tracks.size() - 1, COL__LAST);
        emit dataChanged(index_a, index_b);

        m_database->updateTrackLength(m_tracks.back().start, m_tracks.back().length);

        emit currentTrackTimeChanged(m_tracks.back().length);
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
        m_tracks.back().length = recorded_samples - m_tracks.back().start;

        auto index = createIndex(m_tracks.size() - 1, COL_LENGTH);
        emit dataChanged(index, index);
        emit currentTrackTimeChanged(m_tracks.back().length);
    }
}

void Recording::TrackController::startNewTrack(uint64_t sample_count)
{
    if (m_currentlyRecording) {
        qDebug() << "Beginning track " << m_tracks.size();

        m_tracks.back().length = sample_count - m_tracks.back().start;

        m_database->updateTrackLength(m_tracks.back().start, m_tracks.back().length);

        auto index_a = createIndex(m_tracks.size() - 1, COL__FIRST);
        auto index_b = createIndex(m_tracks.size() - 1, COL__LAST);
        emit dataChanged(index_a, index_b);

        beginInsertRows(QModelIndex(), (int)m_tracks.size(), (int)m_tracks.size());

        Track t { sample_count, 0, QString() };
        m_tracks.push_back(t);

        endInsertRows();

        m_database->insertTrack(sample_count, 0, QString());

        emit currentTrackTimeChanged(0);
    } else {
        qWarning() << "Starting a new track when you're not recording doesn't make any sense "
                   << "and should be disallowed in the user interface. ";
    }
}


int Recording::TrackController::rowCount(const QModelIndex &parent) const
{
    (void)parent;

    return (int)m_tracks.size();
}

int Recording::TrackController::columnCount(const QModelIndex &parent) const
{
    (void)parent;

    return 4;
}

QVariant Recording::TrackController::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (index.row() < 0 || index.row() >= (int)m_tracks.size())
        return QVariant();

    if (role == Qt::DisplayRole) {
        if (index.column() == COL_TRACKNO)
            return index.row();
        else if (index.column() == COL_NAME)
            return m_tracks[index.row()].name;
        else if (index.column() == COL_START)
            return samples_to_time(m_tracks[index.row()].start);
        else if (index.column() == COL_LENGTH)
            return samples_to_time(m_tracks[index.row()].length);
    }

    return QVariant();
}

QVariant Recording::TrackController::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        if (section == COL_TRACKNO)
            return tr("No.");
        else if (section == COL_NAME)
            return tr("Name");
        else if (section == COL_START)
            return tr("Start");
        else if (section == COL_LENGTH)
            return tr("Length");
    } else if (role == Qt::DisplayRole && orientation == Qt::Vertical) {
        return section+1;
    }

    return QVariant();
}

Qt::ItemFlags Recording::TrackController::flags(const QModelIndex &index) const
{
    Qt::ItemFlags flag = 0;

    if (index.column() == COL_NAME)
        flag |= Qt::ItemIsEditable;

    if (index.row() != (int)m_tracks.size()-1 || !m_currentlyRecording)
        flag |= Qt::ItemIsSelectable | Qt::ItemIsEnabled;

    return flag;
}


bool Recording::TrackController::setData(const QModelIndex &index, const QVariant &value, int /*role*/)
{
    if (index.column() == 1 && value.canConvert(QMetaType::QString) && index.row() < (int)m_tracks.size()) {
        m_tracks[index.row()].name = value.toString();
        emit dataChanged(index, index);


        m_database->updateTrackName(m_tracks[index.row()].start, m_tracks[index.row()].name);

        return true;
    }

    return false;
}
