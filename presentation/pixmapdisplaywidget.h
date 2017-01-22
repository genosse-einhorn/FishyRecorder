#ifndef PRESENTATION_PIXMAPDISPLAYWIDGET_H
#define PRESENTATION_PIXMAPDISPLAYWIDGET_H

#include <QWidget>

namespace Presentation {

class PixmapDisplayWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PixmapDisplayWidget(QWidget *parent = 0);

    QPixmap pixmap() const { return m_pixmap; }

signals:
    void pixmapChanged(const QPixmap &pixmap);

public slots:
    void setPixmap(const QPixmap &pixmap);

    // QWidget interface
protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QPixmap m_pixmap;
};

} // namespace Presentation

#endif // PRESENTATION_PIXMAPDISPLAYWIDGET_H
