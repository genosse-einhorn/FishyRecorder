#include "pixmapdisplaywidget.h"

#include <QPaintEvent>
#include <QPainter>

namespace Presentation {

PixmapDisplayWidget::PixmapDisplayWidget(QWidget *parent) : QWidget(parent)
{
    setAutoFillBackground(true);
}

void PixmapDisplayWidget::setPixmap(const QPixmap &pixmap)
{
    m_pixmap = pixmap;
    emit pixmapChanged(pixmap);
    update();
}

} // namespace Presentation


void Presentation::PixmapDisplayWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);

    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    qreal ratioH = qreal(width())/qreal(m_pixmap.width());
    qreal ratioV = qreal(height())/qreal(m_pixmap.height());
    qreal ratio = qMin(ratioH, ratioV);
    qreal w = ratio * m_pixmap.width();
    qreal h = ratio * m_pixmap.height();
    qreal x = (width()-w)/2;
    qreal y = (height()-h)/2;
    painter.drawPixmap(x, y, w, h, m_pixmap);
}
