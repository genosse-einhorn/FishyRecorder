#ifndef RECORDINGSTATUSVIEW_H
#define RECORDINGSTATUSVIEW_H

#include <QWidget>

class QTimer;

namespace Recording {

namespace Ui {
class RecordingStatusView;
}

class StatusView : public QWidget
{
    Q_OBJECT

public:
    explicit StatusView(QWidget *parent = 0);
    ~StatusView();

public slots:
    void handleStatusUpdate(float levelL, float levelR, bool isRecording, qint64 sampleCount);

private slots:
    void blink();

private:
    Ui::RecordingStatusView *ui;
    QTimer *m_blinkTimer;
};

} // namespace Recording

#endif // RECORDINGSTATUSVIEW_H
