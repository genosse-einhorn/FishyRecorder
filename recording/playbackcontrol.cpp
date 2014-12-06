#include "playbackcontrol.h"
#include "ui_playbackcontrol.h"

#include "recording/trackcontroller.h"
#include "recording/playbackslave.h"
#include "recording/trackdataaccessor.h"
#include "error/provider.h"
#include "util/misc.h"

#include <QDebug>
#include <QMetaObject>
#include <QThread>

namespace Recording {

PlaybackControl::PlaybackControl(TrackController *controller, QWidget *parent) :
    QWidget(parent),
    m_ui(new Ui::PlaybackControl),
    m_controller(controller),
    m_playbackSlave(new PlaybackSlave()),
    m_playbackThread(new QThread())
{
    m_ui->setupUi(this);
    m_ui->progressSlider->setRange(0, std::numeric_limits<int>::max());
    m_ui->progressSlider->setTracking(false);

    m_playbackSlave->moveToThread(m_playbackThread);
    QObject::connect(m_playbackThread, &QThread::finished, m_playbackSlave, &QObject::deleteLater);
    m_playbackThread->start();

    QObject::connect(m_playbackSlave->errorProvider(), &Error::Provider::error, m_ui->errorDisplay, &Error::Widget::displayError);
    QObject::connect(m_playbackSlave, &PlaybackSlave::playbackStateChanged, m_ui->playBtn, &QAbstractButton::setChecked);
    QObject::connect(m_ui->playBtn, &QAbstractButton::toggled, m_playbackSlave, SELECT_SIGNAL_OVERLOAD<bool>::OF(&PlaybackSlave::setPlaybackState));
    QObject::connect(m_playbackSlave, &PlaybackSlave::positionChanged, this, &PlaybackControl::positionChanged);
    QObject::connect(m_ui->progressSlider, &QAbstractSlider::sliderReleased, this, &PlaybackControl::sliderUp);
    QObject::connect(m_ui->progressSlider, &QAbstractSlider::sliderPressed, this, &PlaybackControl::sliderDown);
    QObject::connect(m_ui->progressSlider, &QAbstractSlider::sliderMoved, this, &PlaybackControl::sliderPositionChanged);
    QObject::connect(m_ui->splitTrackBtn, &QAbstractButton::clicked, this, &PlaybackControl::splitTrackBtnClicked);
    QObject::connect(m_ui->deleteTrackBtn, &QAbstractButton::clicked, this, &PlaybackControl::deleteTrackBtnClicked);

    trackDeselected();
}

PlaybackControl::~PlaybackControl()
{
    m_playbackThread->quit();
    m_playbackThread->wait(1000*10);
    delete m_playbackThread;

    delete m_ui;
}

void PlaybackControl::trackSelected(unsigned trackNo)
{
    m_currentTrackNo = trackNo;

    auto accessor = m_controller->accessTrackData(trackNo);

    m_trackLength = accessor->getLength();

    accessor->setParent(nullptr);
    accessor->moveToThread(m_playbackThread);

    if (accessor) {
        QMetaObject::invokeMethod(m_playbackSlave, "insertNewSampleSource", Q_ARG(TrackDataAccessor*, accessor), Q_ARG(uint64_t, 0));

        this->setEnabled(true);
    } else {
        qWarning() << "Invalid track number" << trackNo;

        trackDeselected();
    }
}

void PlaybackControl::trackDeselected()
{
    m_ui->playBtn->setChecked(false);
    m_ui->progressSlider->setValue(m_ui->progressSlider->minimum());
    m_ui->timeLbl->setText(tr("No track selected"));

    this->setEnabled(false);
}

void PlaybackControl::monitorDeviceChanged(PaDeviceIndex index)
{
    QMetaObject::invokeMethod(m_playbackSlave, "setMonitorDevice", Q_ARG(PaDeviceIndex, index));
}

void PlaybackControl::positionChanged(uint64_t position)
{
    // update the slider
    double pos = (double)position/(double)m_trackLength;

    int sliderVal = (int)(pos*m_ui->progressSlider->maximum());

    m_ui->progressSlider->blockSignals(true);
    m_ui->progressSlider->setValue(sliderVal);
    m_ui->progressSlider->blockSignals(false);

    // update the time label
    updateTime(position);
}

void PlaybackControl::sliderDown()
{
    QMetaObject::invokeMethod(m_playbackSlave, "setPlaybackState", Q_ARG(bool, false), Q_ARG(bool, false));
}

void PlaybackControl::sliderUp()
{
    double pos = (double)m_ui->progressSlider->sliderPosition()/(double)m_ui->progressSlider->maximum();

    uint64_t samples = (uint64_t)(pos * (double)m_trackLength);

    QMetaObject::invokeMethod(m_playbackSlave, "seek", Q_ARG(uint64_t, samples));
    QMetaObject::invokeMethod(m_playbackSlave, "setPlaybackState", Q_ARG(bool, m_ui->playBtn->isChecked()));
}

void PlaybackControl::sliderPositionChanged()
{
    if (m_ui->progressSlider->isSliderDown())
        return;

    double pos = (double)m_ui->progressSlider->sliderPosition()/(double)m_ui->progressSlider->maximum();

    uint64_t samples = (uint64_t)(pos * (double)m_trackLength);

    updateTime(samples);
}

void PlaybackControl::splitTrackBtnClicked()
{
    uint64_t position = 0;
    QMetaObject::invokeMethod(m_playbackSlave, "position", Qt::BlockingQueuedConnection, Q_RETURN_ARG(uint64_t, position));

    if (position == 0 || position >= m_trackLength) {
        qWarning() << "Tried to split track at the beginning => ignoring request";
        return;
    }

    m_controller->splitTrack(m_currentTrackNo, position);

    trackSelected(m_currentTrackNo);
}

void PlaybackControl::deleteTrackBtnClicked()
{
    m_controller->deleteTrack(m_currentTrackNo);

    trackDeselected();
}

void PlaybackControl::updateTime(uint64_t position)
{
    uint64_t total_seconds = (m_trackLength/44100) % 60;
    uint64_t total_minutes = m_trackLength/44100/60 % 60;
    uint64_t total_hours   = m_trackLength/44100/60/60;

    QString total_str = QString("%1:%2:%3").arg(total_hours).arg(total_minutes, 2, 10, QChar('0')).arg(total_seconds, 2, 10, QChar('0'));

    uint64_t seconds = (position/44100) % 60;
    uint64_t minutes = position/44100/60 % 60;
    uint64_t hours   = position/44100/60/60;

    QString current_str = QString("%1:%2:%3").arg(hours).arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));

    m_ui->timeLbl->setText(QString("<b>%1</b> / %2").arg(current_str).arg(total_str));
}

} // namespace Recording
