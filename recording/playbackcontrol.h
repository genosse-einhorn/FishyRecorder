#ifndef RECORDING_PLAYBACKCONTROL_H
#define RECORDING_PLAYBACKCONTROL_H

#include <QWidget>

#include <portaudio.h>

namespace Recording {

class TrackController;
class PlaybackSlave;

namespace Ui {
class PlaybackControl;
}

class PlaybackControl : public QWidget
{
    Q_OBJECT

public:
    explicit PlaybackControl(TrackController *controller, QWidget *parent = 0);
    ~PlaybackControl();

public slots:
    void trackSelected(unsigned trackNo);
    void trackDeselected();
    void monitorDeviceChanged(PaDeviceIndex index);

private slots:
    void positionChanged(uint64_t position);
    void sliderDown();
    void sliderUp();
    void sliderPositionChanged();
    void splitTrackBtnClicked();
    void deleteTrackBtnClicked();

private:
    void updateTime(uint64_t position);

    Ui::PlaybackControl *m_ui;

    TrackController *m_controller;
    PlaybackSlave   *m_playbackSlave;
    QThread         *m_playbackThread;
    uint64_t         m_trackLength;
    unsigned         m_currentTrackNo;
};


} // namespace Recording
#endif // RECORDING_PLAYBACKCONTROL_H
