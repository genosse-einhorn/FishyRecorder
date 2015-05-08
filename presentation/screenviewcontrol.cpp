#include "screenviewcontrol.h"
#include "screenviewrenderer.h"

#include <QTimer>
#include <QThread>
#include <QMetaObject>

Presentation::ScreenViewControl::ScreenViewControl(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_PaintOnScreen, true);
    setAttribute(Qt::WA_NativeWindow, true);

    m_thread = new QThread(this);
}

Presentation::ScreenViewControl::~ScreenViewControl()
{
    if (m_renderer) {
        QMetaObject::invokeMethod(m_renderer, "stop");
    }

    m_thread->quit();
    m_thread->wait();
}

void Presentation::ScreenViewControl::resizeEvent(QResizeEvent *evt)
{
    Q_UNUSED(evt);

    if (m_renderer) {
        QMetaObject::invokeMethod(m_renderer, "windowResized", Q_ARG(QRect, QRect(0, 0, this->width(), this->height())));
    }
}

void Presentation::ScreenViewControl::paintEvent(QPaintEvent *evt)
{
    QWidget::paintEvent(evt);
}

void Presentation::ScreenViewControl::screenUpdated(const QRect &rect)
{
    if (m_renderer) {
        QMetaObject::invokeMethod(m_renderer, "updateScreen", Q_ARG(QRect, rect));
    } else {
        m_renderer = new ScreenViewRenderer((HWND)winId(), rect);
        m_renderer->moveToThread(m_thread);
        QObject::connect(m_renderer, &ScreenViewRenderer::stopped, m_thread, &QThread::quit);
        QObject::connect(m_thread, &QThread::finished, m_renderer, &QObject::deleteLater);

        m_thread->start();
        QMetaObject::invokeMethod(m_renderer, "startRendering");
    }
}

void Presentation::ScreenViewControl::setExcludedWindow(HWND win)
{
    Q_UNUSED(win);

    //FIXME: unimplementable
}


