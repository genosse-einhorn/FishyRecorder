#include "tracklistmodel.h"

#include <recording/trackcontroller.h>

static QString samples_to_time(uint64_t n_samples)
{
    uint64_t minutes = n_samples/44100/60;
    uint64_t seconds = n_samples/44100 % 60;

    return QString("%1:%2").arg(minutes).arg(seconds, 2, 10, QChar('0'));
}

namespace Recording {

TrackListModel::TrackListModel(TrackController *controller, QObject *parent)
    : QAbstractTableModel(parent), m_controller(controller)
{
    QObject::connect(controller, &TrackController::beforeTrackAdded, this, &TrackListModel::handleBeforeTrackAdded);
    QObject::connect(controller, &TrackController::trackAdded, this, &TrackListModel::handleTrackAdded);
    QObject::connect(controller, &TrackController::beforeTrackRemoved, this, &TrackListModel::handleBeforeTrackRemoved);
    QObject::connect(controller, &TrackController::trackRemoved, this, &TrackListModel::handleTrackRemoved);
    QObject::connect(controller, &TrackController::trackChanged, this, &TrackListModel::handleTrackChanged);
}

TrackListModel::~TrackListModel()
{

}

void TrackListModel::handleBeforeTrackAdded(int trackIndex)
{
    beginInsertRows(QModelIndex(), trackIndex, trackIndex);
}

void TrackListModel::handleTrackAdded(int)
{
    endInsertRows();
}

void TrackListModel::handleBeforeTrackRemoved(int trackIndex)
{
    beginRemoveRows(QModelIndex(), trackIndex, trackIndex);
}

void TrackListModel::handleTrackRemoved(int)
{
    endRemoveRows();
}

void TrackListModel::handleTrackChanged(int trackIndex)
{
    auto index_a = createIndex(trackIndex, COL__FIRST);
    auto index_b = createIndex(trackIndex, COL__LAST);
    emit dataChanged(index_a, index_b);
}

} // namespace Recording



int Recording::TrackListModel::rowCount(const QModelIndex &) const
{
    return (int)m_controller->trackCount();
}

int Recording::TrackListModel::columnCount(const QModelIndex &) const
{
    return 1 + COL__LAST - COL__FIRST;
}

QVariant Recording::TrackListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (index.row() < 0 || index.row() >= m_controller->trackCount())
        return QVariant();

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        if (index.column() == COL_TRACKNO)
            return index.row();
        else if (index.column() == COL_NAME)
            return m_controller->trackName(index.row());
        else if (index.column() == COL_START)
            return samples_to_time(m_controller->trackStart(index.row()));
        else if (index.column() == COL_LENGTH)
            return samples_to_time(m_controller->trackLength(index.row()));
        else if (index.column() == COL_TIMESTAMP)
            return m_controller->trackTimestamp(index.row()).toLocalTime().toString(Qt::SystemLocaleShortDate);
    }

    return QVariant();
}

bool Recording::TrackListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    Q_UNUSED(role);

    if (index.column() == 1 && value.canConvert(QMetaType::QString) && index.row() < m_controller->trackCount()) {
        m_controller->setTrackName(index.row(), value.toString());

        emit dataChanged(index, index);

        return true;
    }

    return false;
}

QVariant Recording::TrackListModel::headerData(int section, Qt::Orientation orientation, int role) const
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
        else if (section == COL_TIMESTAMP)
            return tr("Timestamp");
    } else if (role == Qt::DisplayRole && orientation == Qt::Vertical) {
        return section+1;
    }

    return QVariant();
}

Qt::ItemFlags Recording::TrackListModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags flag = 0;

    if (index.column() == COL_NAME)
        flag |= Qt::ItemIsEditable;

    if (!m_controller->trackBeingRecorded(index.row()))
        flag |= Qt::ItemIsSelectable | Qt::ItemIsEnabled;

    return flag;
}
