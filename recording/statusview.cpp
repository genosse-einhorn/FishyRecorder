#include "statusview.h"
#include "ui_recordingstatusview.h"

#include <QTimer>

namespace Recording {

StatusView::StatusView(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::RecordingStatusView)
{
    ui->setupUi(this);

    m_blinkTimer = new QTimer(this);
    m_blinkTimer->setInterval(1000);
    QObject::connect(m_blinkTimer, &QTimer::timeout, this, &StatusView::blink);

    handleStatusUpdate(0, 0, false, 0);
}

StatusView::~StatusView()
{
    delete ui;
}

void StatusView::handleStatusUpdate(float levelL, float levelR, bool isRecording, qint64 sampleCount)
{
    ui->meterL->setValue(levelL);
    ui->meterR->setValue(levelR);

    if (isRecording)
    {
        ui->lStatus->setText(tr("RECORDING"));
        if (!m_blinkTimer->isActive())
            m_blinkTimer->start();

        qint64 samples = sampleCount;
        qint64 seconds = (samples / 48000) % 60;
        qint64 minutes = (samples / 48000 / 60) % 60;
        qint64 hours = samples / 48000 / 60 / 60;

        ui->lTime->setText(QString("%1:%2:%3")
                              .arg(hours)
                              .arg(minutes, 2, 10, QChar('0'))
                              .arg(seconds, 2, 10, QChar('0')));
    }
    else
    {
        ui->lStatus->setVisible(true);
        ui->lStatus->setText(tr("STOPPED"));
        m_blinkTimer->stop();

        ui->lTime->setText(QString());
    }
}

void StatusView::blink()
{
    ui->lStatus->setVisible(!ui->lStatus->isVisible());
}

} // namespace Recording
