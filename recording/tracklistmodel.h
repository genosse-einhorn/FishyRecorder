#ifndef RECORDING_TRACKLISTMODEL_H
#define RECORDING_TRACKLISTMODEL_H

#include <QObject>
#include <QAbstractTableModel>

namespace Recording {

class TrackController;

class TrackListModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit TrackListModel(TrackController *controller, QObject *parent = 0);
    ~TrackListModel();

    enum ColumnNumbers {
        COL_TRACKNO   = 0,
        COL_NAME      = 1,
        COL_START     = 2,
        COL_LENGTH    = 3,
        COL_TIMESTAMP = 4,

        COL__FIRST    = COL_TRACKNO,
        COL__LAST     = COL_TIMESTAMP
    };

private:
    TrackController *m_controller = nullptr;

private slots:
    void handleBeforeTrackAdded(int newTrackIndex);
    void handleTrackAdded(int trackIndex);
    void handleBeforeTrackRemoved(int trackIndex);
    void handleTrackRemoved(int trackIndex);
    void handleTrackChanged(int trackIndex);

    // QAbstractItemModel interface
public:
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
};

} // namespace Recording

#endif // RECORDING_TRACKLISTMODEL_H
