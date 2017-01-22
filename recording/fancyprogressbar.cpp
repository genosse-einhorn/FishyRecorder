#include "fancyprogressbar.h"

#include <QPainter>
#include <cmath>

namespace Recording {

FancyProgressBar::FancyProgressBar(QWidget *parent) :
    QWidget(parent)
{
}

void FancyProgressBar::setValue(float value)
{
    m_value = value;

    update();
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
    int boxWidth = (int)(width() * std::pow(m_value, 1.0f/3.0f));

    QPainter painter(this);

    if (boxWidth > width()) {
        painter.fillRect(0, 0, width(), height(), palette().brush(palette().currentColorGroup(), QPalette::Highlight));
    } else {
        painter.fillRect(0, 0, width(), height(), palette().brush(palette().currentColorGroup(), QPalette::Background));
        painter.fillRect(0, 0, boxWidth, height(), palette().brush(palette().currentColorGroup(), QPalette::Foreground));
    }
}

} // namespace Recording
