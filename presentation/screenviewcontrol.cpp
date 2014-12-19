#include "screenviewcontrol.h"

#include <windows.h>
#include <QLibrary>
#include <QDebug>
#include <QGridLayout>
#include <QLabel>
#include <QTimer>

namespace {

typedef struct tagMAGTRANSFORM {
  float v[3][3];
} MAGTRANSFORM, *PMAGTRANSFORM;

#define MS_SHOWMAGNIFIEDCURSOR 0x0001L

BOOL (*MagInitialize)();
BOOL (*MagUninitialize)();
BOOL (*MagSetWindowSource)(HWND hwnd, RECT rect);
BOOL (*MagSetWindowTransform)(HWND hwnd, PMAGTRANSFORM pTransform);
BOOL (*MagSetWindowFilterList)(HWND hwnd, DWORD dwFilterMode, int count, HWND *pHWND);

bool initializeMagnificationApi() {
    QLibrary lib("Magnification.dll");

    if (!lib.load()) {
        qWarning() << "Magnification.dll is missing :(";
        return false;
    }

#define RESOLVE(func) func = (decltype(func)) lib.resolve(#func)

    RESOLVE(MagInitialize);
    RESOLVE(MagUninitialize);
    RESOLVE(MagSetWindowSource);
    RESOLVE(MagSetWindowTransform);
    RESOLVE(MagSetWindowFilterList);

#undef RESOLVE

    return MagInitialize();
}

}

namespace Presentation {

ScreenViewControl::ScreenViewControl(QWidget *parent) :
    QWinHost(parent)
{
    if (!initializeMagnificationApi()) {
        qWarning() << "Failed to initialize magnification api :(";

        QGridLayout *l = new QGridLayout(this);
        l->addWidget(new QLabel("Magnification API not ready", this));

        this->setLayout(l);
    } else {
        this->m_magApiReady = true;
    }

    m_resizeTimer = new QTimer(this);
    QObject::connect(m_resizeTimer, &QTimer::timeout, this, &ScreenViewControl::doResize);

    QTimer *invalidateTimer = new QTimer(this);
    QObject::connect(invalidateTimer, &QTimer::timeout, this, &ScreenViewControl::invalidateScreen);
    invalidateTimer->start(50);
}

void ScreenViewControl::screenUpdated(const QRect &rect)
{
    m_screen = rect;

    qDebug() << "Screen has been set to" << rect;

    if (!m_magApiReady || !this->window() || !m_screen.width())
        return;

    doResize();
}

void ScreenViewControl::doResize()
{
    m_resizeTimer->stop();

    if (window())
        ::SetWindowPos(window(), HWND_TOP, 0, 0, width(), height(), 0);

    if (!m_magApiReady || !m_screen.width())
        return;

    float factorX = ((float)width()/m_screen.width());
    float factorY = ((float)height()/m_screen.height());

    float factor = qMin(factorX, factorY);

    MAGTRANSFORM transform;
    memset(&transform, 0, sizeof(MAGTRANSFORM));
    transform.v[0][0] = factor;
    transform.v[1][1] = factor;
    transform.v[2][2] = 1.0f;

    RECT srcRect { m_screen.left(), m_screen.top(), m_screen.width(), m_screen.height() };

    qDebug() << "Setting transformation factor" << factor;

    MagSetWindowTransform(window(), &transform);
    MagSetWindowSource(window(), srcRect);
}

void ScreenViewControl::invalidateScreen()
{
    if (!window() || !m_screen.width())
        return;

    ::InvalidateRect(window(), nullptr, true);
}

} // namespace Presentation


HWND Presentation::ScreenViewControl::createWindow(HWND parent, HINSTANCE instance)
{
    if (!m_magApiReady)
        return NULL;

    HWND mag = CreateWindow(L"Magnifier", L"MagnifierWindow",
            WS_CHILD | MS_SHOWMAGNIFIEDCURSOR | WS_VISIBLE,
            this->x(), this->y(), this->width()+this->x(), this->height()+this->y(), parent, NULL, instance, NULL );

    return mag;
}


void Presentation::ScreenViewControl::resizeEvent(QResizeEvent *ev)
{
    // skip the window host, but chain up to qwidget right away
    QWidget::resizeEvent(ev);

    m_resizeTimer->stop();
    m_resizeTimer->start(250);
}
