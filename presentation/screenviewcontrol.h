#ifndef PRESENTATION_SCREENVIEWCONTROL_H
#define PRESENTATION_SCREENVIEWCONTROL_H

#include "external/qwinhost.h"

class QTimer;

namespace Presentation {

class ScreenViewControl : public QWinHost
{
    Q_OBJECT
public:
    explicit ScreenViewControl(QWidget *parent = 0);
    ~ScreenViewControl();

signals:

public slots:
    void screenUpdated(const QRect& rect);

private slots:
    void doResize();
    void invalidateScreen();

private:
    QRect   m_screen          = QRect(0, 0, 0, 0);
    bool    m_magApiReady     = false;
    QTimer *m_resizeTimer     = nullptr;
    QTimer *m_invalidateTimer = nullptr;

    // QWinHost interface
protected:
    virtual HWND createWindow(HWND parent, HINSTANCE instance);

    // QWidget interface
protected:
    virtual void resizeEvent(QResizeEvent *);
};

} // namespace Presentation

#endif // PRESENTATION_SCREENVIEWCONTROL_H
