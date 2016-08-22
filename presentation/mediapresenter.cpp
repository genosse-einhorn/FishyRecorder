#include "mediapresenter.h"
#include "ui_mediapresenter.h"

#include <QMediaPlayer>
#include <QVideoWidget>
#include <QMediaService>
#include <QAudioOutputSelectorControl>
#include <QMessageBox>

namespace Presentation {

MediaPresenter::MediaPresenter(const QString &file, QWidget *parent) :
    PresenterBase(parent),
    ui(new Ui::MediaPresenter)
{
    ui->setupUi(this);

    m_player = new QMediaPlayer(this);
    QObject::connect(m_player, &QMediaPlayer::durationChanged, this, &MediaPresenter::handleDuration);
    QObject::connect(m_player, &QMediaPlayer::positionChanged, this, &MediaPresenter::handlePosition);
    QObject::connect(m_player, &QMediaPlayer::stateChanged, this, &MediaPresenter::handleStateChange);
    QObject::connect(m_player, &QMediaPlayer::volumeChanged, this, &MediaPresenter::handleVolumeChange);
    QObject::connect(m_player, static_cast<void(QMediaPlayer::*)(QMediaPlayer::Error)>(&QMediaPlayer::error),
                     this, &MediaPresenter::handleError);
    QObject::connect(ui->seekSlider, &QSlider::sliderMoved, this, &MediaPresenter::handleSeek);
    QObject::connect(ui->playBtn, &QAbstractButton::clicked, this, &MediaPresenter::handlePlayClick);
    QObject::connect(ui->muteButton, &QAbstractButton::clicked, this, &MediaPresenter::handleMuteClicked);
    QObject::connect(ui->volumeSlider, &QSlider::sliderMoved, this, &MediaPresenter::handleVolumeSliderMoved);

    m_player->setMedia(QMediaContent(QUrl::fromLocalFile(file)));

    m_loudIcon = QIcon(":/images/audio-volume-medium.svg");
    m_muteIcon = QIcon(":/images/audio-volume-muted.svg");

    // Prime volume and audio device selection
    // TODO: Save in config db
    m_player->setVolume(0);
    m_player->setVolume(100);

    m_audioOutputControl = qobject_cast<QAudioOutputSelectorControl*>
            (m_player->service()->requestControl(QAudioOutputSelectorControl_iid));
    if (m_audioOutputControl) {
        m_audioOutputControl->setActiveOutput(m_audioOutputControl->defaultOutput());
        QObject::connect(m_audioOutputControl, &QAudioOutputSelectorControl::activeOutputChanged, this, &MediaPresenter::updateAudioOutputCombo);
        QObject::connect(m_audioOutputControl, &QAudioOutputSelectorControl::availableOutputsChanged, this, &MediaPresenter::updateAudioOutputCombo);

        this->updateAudioOutputCombo();

        QObject::connect(ui->audioDeviceCombo, &QComboBox::currentTextChanged, this, &MediaPresenter::handleAudioOutputDeviceChanged);
    } else {
        ui->audioDeviceCombo->setEnabled(false);
    }

    m_videowidget = new QVideoWidget;
    m_player->setVideoOutput(m_videowidget);
}

MediaPresenter::~MediaPresenter()
{
    m_player->stop();
    m_player->service()->releaseControl(m_audioOutputControl);

    delete ui;
    delete m_videowidget;
}

void MediaPresenter::handleDuration(qint64 duration)
{
    ui->seekSlider->setEnabled(true);
    ui->seekSlider->setMinimum(0);
    ui->seekSlider->setMaximum(int(duration / 1000)); //FIXME int truncation
}

static inline QString formatTime(uint64_t msecs) {
    uint64_t seconds = msecs/1000 % 60;
    uint64_t minutes = msecs/1000/60;

    return QString("%1:%2").arg(minutes).arg(seconds, 2, 10, QChar('0'));
}

void MediaPresenter::handlePosition(qint64 position)
{
    if (!ui->seekSlider->isSliderDown()) {
        ui->seekSlider->setValue(int(position / 1000)); //FIXME int truncation
    }

    qint64 eta = m_player->duration() - position;

    ui->timeLbl->setText(formatTime(position));
    ui->etaLbl->setText(QString("-%1").arg(formatTime(eta)));
}

void MediaPresenter::handleSeek(int secs)
{
    m_player->setPosition(qint64(secs) * 1000);
}

void MediaPresenter::handleError()
{
    if (m_player->errorString().size()) {
        QMessageBox::critical(this, "DEBUG Error", m_player->errorString());
    }
}

void MediaPresenter::handlePlayClick()
{
    if (ui->playBtn->isChecked()) {
        m_player->play();

        if (m_player->isVideoAvailable())
            emit requestPresentation(m_videowidget);
    } else {
        m_player->pause();
    }
}

void MediaPresenter::handleStateChange()
{
    switch (m_player->state()) {
    case QMediaPlayer::PlayingState:
        ui->playBtn->setChecked(true);
        break;
    case QMediaPlayer::PausedState:
        ui->playBtn->setChecked(false);
        break;
    case QMediaPlayer::StoppedState:
        ui->playBtn->setChecked(false);
        m_player->setPosition(0);
        break;
    }
}

void MediaPresenter::handleMuteClicked()
{
    if (m_player->volume() == 0) {
        // unmute
        m_player->setVolume(m_oldvolume);
        m_oldvolume = 100;
    } else {
        // mute
        m_oldvolume = m_player->volume();
        m_player->setVolume(0);
    }
}

void MediaPresenter::handleVolumeChange()
{
    if (!ui->volumeSlider->isSliderDown()) {
        ui->volumeSlider->setValue(m_player->volume());
    }

    if (m_player->volume() > 0) {
        ui->muteButton->setIcon(m_loudIcon);
    } else {
        ui->muteButton->setIcon(m_muteIcon);
    }
}

void MediaPresenter::handleVolumeSliderMoved()
{
    m_player->setVolume(ui->volumeSlider->value());
}

void MediaPresenter::handleAudioOutputDeviceChanged()
{
    m_audioOutputControl->setActiveOutput(ui->audioDeviceCombo->currentData().toString());
}

void MediaPresenter::updateAudioOutputCombo()
{
    if (!m_audioOutputControl)
        return;

    ui->audioDeviceCombo->clear();
    for (QString output : m_audioOutputControl->availableOutputs()) {
        ui->audioDeviceCombo->addItem(m_audioOutputControl->outputDescription(output), QVariant::fromValue(output));
    }
    ui->audioDeviceCombo->setCurrentText(m_audioOutputControl->activeOutput());
}

} // namespace Presentation


QString Presentation::MediaPresenter::title()
{
    return m_player->media().canonicalUrl().fileName();
}

void Presentation::MediaPresenter::tabHidden()
{
    // It may make sense to keep audio playing in the background,
    // but we don't believe that having video in the background would
    // make anything better
    if (m_player->isVideoAvailable())
        m_player->pause();
}

void Presentation::MediaPresenter::tabVisible()
{
    if (m_player->isVideoAvailable())
        emit requestPresentation(m_videowidget);
}
