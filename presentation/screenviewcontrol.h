#ifndef PRESENTATION_SCREENVIEWCONTROL_H
#define PRESENTATION_SCREENVIEWCONTROL_H

#include <QWidget>

class QThread;

namespace Presentation {

class ScreenViewRenderer;

class ScreenViewControl : public QWidget
{
    Q_OBJECT
public:
    explicit ScreenViewControl(QWidget *parent = 0);
    ~ScreenViewControl();

    QPaintEngine* paintEngine() const override { return nullptr; }

protected:
      void resizeEvent(QResizeEvent* evt) override;
      void paintEvent(QPaintEvent* evt) override;

signals:

public slots:
    void screenUpdated(const QRect& rect);
#ifdef Q_OS_WIN32
    void setExcludedWindow(HWND win);
#endif

private:
    QThread *m_thread = nullptr;
    ScreenViewRenderer *m_renderer = nullptr;
};

} // namespace Presentation

#endif // PRESENTATION_SCREENVIEWCONTROL_H
