#ifndef TRACKRECORDINGCONTROLLER_H
#define TRACKRECORDINGCONTROLLER_H

#include <QAbstractTableModel>
#include <QDateTime>
#include <cstdint>

namespace Config {
    class Database;
}

namespace Recording {

class TrackDataAccessor;

class TrackController : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit TrackController(Config::Database *db, QObject *parent = 0);

    unsigned getTrackCount() const;
    QString  getTrackName(unsigned trackIndex) const;
    uint64_t getTrackStart(unsigned trackIndex) const;
    uint64_t getTrackLength(unsigned trackIndex) const;

    void setTrackName(unsigned trackIndex, const QString& name);
    void splitTrack(unsigned trackIndex, uint64_t after_n_samples);
    void deleteTrack(unsigned trackIndex);

    TrackDataAccessor *accessTrackData(unsigned trackId) const;

    enum ColumnNumbers {
        COL_TRACKNO   = 0,
        COL_NAME      = 1,
        COL_START     = 2,
        COL_LENGTH    = 3,
        COL_TIMESTAMP = 4,

        COL__FIRST    = COL_TRACKNO,
        COL__LAST     = COL_TIMESTAMP
    };

signals:
    void currentTrackTimeChanged(uint64_t n_samples);
    void trackListChanged();
    void trackNameChanged(unsigned trackIndex, const QString& name);

public slots:
    void onRecordingStateChanged(bool recording, uint64_t n_samples);
    void onRecordingFileChanged(const QString& new_file_path, uint64_t starting_sample_count);
    void onTimeUpdate(uint64_t recorded_samples);
    void startNewTrack(uint64_t sample_count);

    // QAbstractItemModel interface
public:
    virtual int rowCount(const QModelIndex &parent) const;
    virtual int columnCount(const QModelIndex &parent) const;
    virtual QVariant data(const QModelIndex &index, int role) const;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    virtual Qt::ItemFlags flags(const QModelIndex &index) const;

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

    // QAbstractItemModel interface
public:
    virtual bool setData(const QModelIndex &index, const QVariant &value, int role);
};

} // namespace Recording

#endif // TRACKRECORDINGCONTROLLER_H
