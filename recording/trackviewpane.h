#ifndef RECORDING_TRACKVIEWPANE_H
#define RECORDING_TRACKVIEWPANE_H

#include <QWidget>

#include <portaudio.h>

class QTableView;

namespace Recording {

class TrackController;
class PlaybackControl;

class TrackViewPane : public QWidget
{
    Q_OBJECT
public:
    explicit TrackViewPane(TrackController *controller, QWidget *parent = 0);

signals:

public slots:
    void setMonitorDevice(PaDeviceIndex index);

private slots:
    void selectedRowChanged();

private:
    TrackController *m_controller = nullptr;
    PlaybackControl *m_playback   = nullptr;
    QTableView      *m_trackView  = nullptr;
};

} // namespace Recording

#endif // RECORDING_TRACKVIEWPANE_H
