#ifndef PRESENTATION_MEDIAPRESENTER_H
#define PRESENTATION_MEDIAPRESENTER_H

#include "presentation/presenterbase.h"

#include <QIcon>

class QMediaPlayer;
class QVideoWidget;
class QAudioOutputSelectorControl;

namespace Presentation {

namespace Ui {
class MediaPresenter;
}

class MediaPresenter : public PresenterBase
{
    Q_OBJECT

public:
    explicit MediaPresenter(const QString &file, QWidget *parent = 0);
    ~MediaPresenter();

private:
    Ui::MediaPresenter *ui;

    QMediaPlayer *m_player { nullptr };
    QAudioOutputSelectorControl *m_audioOutputControl { nullptr };
    QVideoWidget *m_videowidget { nullptr };

    QIcon m_loudIcon;
    QIcon m_muteIcon;
    int m_oldvolume { 100 };

private slots:
    void handleDuration(qint64 duration);
    void handlePosition(qint64 position);
    void handleSeek(int secs);
    void handleError();
    void handlePlayClick();
    void handleStateChange();
    void handleMuteClicked();
    void handleVolumeChange();
    void handleVolumeSliderMoved();
    void handleAudioOutputDeviceChanged();

    void updateAudioOutputCombo();

    // PresenterBase interface
public:
    QString title() override;

public slots:
    void tabHidden() override;
    void tabVisible() override;
};


} // namespace Presentation
#endif // PRESENTATION_MEDIAPRESENTER_H
