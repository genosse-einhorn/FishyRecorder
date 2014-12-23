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

    painter.setPen(palette().color(palette().currentColorGroup(), QPalette::Light));
    painter.drawRect(0, 0, width()-1, height()-1);
    painter.fillRect(0, 0, boxWidth, height(), palette().brush(palette().currentColorGroup(), QPalette::Foreground));
}
