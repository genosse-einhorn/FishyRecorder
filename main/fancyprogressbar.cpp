#include "fancyprogressbar.h"

#include <QPainter>

FancyProgressBar::FancyProgressBar(QWidget *parent) :
    QProgressBar(parent)
{
}


QSize FancyProgressBar::sizeHint() const
{
    return QSize(50, fontMetrics().height() + 2);
}

QSize FancyProgressBar::minimumSizeHint() const
{
    return QSize(10, fontMetrics().height() + 2);
}

void FancyProgressBar::paintEvent(QPaintEvent *)
{
    int boxWidth = (int)((double)width() * (value()-minimum())/(maximum()-minimum()));

    QPainter painter(this);

    painter.fillRect(0, 0, width(), height(), palette().brush(palette().currentColorGroup(), QPalette::Light));
    painter.fillRect(0, 0, boxWidth, height(), palette().brush(palette().currentColorGroup(), QPalette::Foreground));
}
