#include "screenviewcontrol.h"

#include "util/misc.h"

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

enum FilterMode
{
    MW_FILTERMODE_EXCLUDE = 0,
    MW_FILTERMODE_INCLUDE = 1
};

#define MS_SHOWMAGNIFIEDCURSOR 0x0001L

BOOL (WINAPI *MagInitialize)() = nullptr;
BOOL (WINAPI *MagUninitialize)();
BOOL (WINAPI *MagSetWindowSource)(HWND hwnd, RECT rect);
BOOL (WINAPI *MagSetWindowTransform)(HWND hwnd, PMAGTRANSFORM pTransform);
BOOL (WINAPI *MagSetWindowFilterList)(HWND hwnd, DWORD dwFilterMode, int count, HWND *pHWND);

bool initializeMagnificationApi() {
    if (MagInitialize)
        return true;

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

        return;
    } else {
        this->m_magApiReady = true;
    }

    m_resizeTimer = new QTimer(this);
    QObject::connect(m_resizeTimer, &QTimer::timeout, this, &ScreenViewControl::doResize);

    m_invalidateTimer = new QTimer(this);
    QObject::connect(m_invalidateTimer, &QTimer::timeout, this, &ScreenViewControl::invalidateScreen);
    m_invalidateTimer->setInterval(50);

    // create a window class for the separate host window
    WNDCLASSEX wcex;
    memset(&wcex, 0, sizeof wcex);
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = DefWindowProc;
    wcex.hInstance      = GetModuleHandle(NULL);
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(1 + COLOR_BTNFACE);
    wcex.lpszClassName  = L"MagnifierWindow";

    RegisterClassEx(&wcex); // ignore error

    // create magnification window
    m_hostWindow = CreateWindowEx(WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT,
                                  L"MagnifierWindow", L"Magnifier Window",
                                  WS_CLIPCHILDREN | WS_POPUP,
                                  0, 0, 300, 300, NULL, NULL, qWinAppInst(), NULL);
    if (!m_hostWindow)
    {
        qWarning() << "Error CreateWindowEx (host window)" << GetLastError();
    }

    // Make the window opaque.
    SetLayeredWindowAttributes(m_hostWindow, 0, 255, LWA_ALPHA);

    // Create a magnifier control that fills the client area.
    RECT magWindowRect;
    GetClientRect(m_hostWindow, &magWindowRect);
    m_magnifier = CreateWindow(L"Magnifier", TEXT("MagnifierWindow"),
                               WS_CHILD | MS_SHOWMAGNIFIEDCURSOR | WS_VISIBLE,
                               magWindowRect.left, magWindowRect.top, magWindowRect.right, magWindowRect.bottom,
                               m_hostWindow, NULL, qWinAppInst(), NULL );
    if (!m_magnifier)
    {
        qWarning() << "Error CreateWindow (Magnifier control)" << GetLastError();
    }
}

ScreenViewControl::~ScreenViewControl()
{
    if (m_magApiReady)
        MagUninitialize();
}

void ScreenViewControl::screenUpdated(const QRect &rect)
{
    m_screen = rect;

    qDebug() << "Screen has been set to" << rect;

    if (!m_magApiReady || !m_screen.width())
        return;

    RECT r { rect.left(), rect.top(), rect.right() + 1, rect.bottom() + 1 };
    if (!MagSetWindowSource(m_magnifier, r))
        qWarning() << "Failed: MagSetWindowSource";
}

void ScreenViewControl::setExcludedWindow(HWND win)
{
    m_excludedWindow = win;

    if (!m_magApiReady)
        return;

    qDebug() << "Setting excluded window" << win;
    HWND excludedWindows[] = { win, (HWND)this->topLevelWidget()->winId() };
    if (!MagSetWindowFilterList(m_magnifier, MW_FILTERMODE_EXCLUDE, 2, excludedWindows))
        qWarning() << "MagSetWindowFilterList failed!" << GetLastError();
}

void ScreenViewControl::doResize()
{
    m_resizeTimer->stop();
    m_invalidateTimer->start();

    if (!m_magApiReady || !m_screen.width())
        return;

    // calculate and set a new magnification factor
    float factorX = ((float)width()/m_screen.width());
    float factorY = ((float)height()/m_screen.height());

    float factor = qMin(factorX, factorY);

    MAGTRANSFORM transform;
    memset(&transform, 0, sizeof(MAGTRANSFORM));
    transform.v[0][0] = factor;
    transform.v[1][1] = factor;
    transform.v[2][2] = 1.0f;

    qDebug() << "Setting transformation factor" << factor;

    MEASURE_TIME(MagSetWindowTransform(m_magnifier, &transform));
    RECT r = { m_screen.left(), m_screen.top(), m_screen.right()+1, m_screen.bottom()+1 };
    MEASURE_TIME(MagSetWindowSource(m_magnifier, r));

    // resize hosting window
    auto res = SetWindowPos(m_hostWindow, NULL, 0, 0, width(), height(), SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_NOZORDER);
    if (!res)
        qWarning() << "SetWindowPos for host window failed:" << GetLastError();

    // resize the magnifier control inside the host window
    RECT cr;
    ::GetClientRect(m_hostWindow, &cr);
    MEASURE_TIME(SetWindowPos(m_magnifier, NULL, cr.left, cr.top, cr.right-cr.left, cr.bottom-cr.top, SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW));


}

void ScreenViewControl::invalidateScreen()
{
    ::InvalidateRect(m_magnifier, nullptr, true);
}

} // namespace Presentation


HWND Presentation::ScreenViewControl::createWindow(HWND parent, HINSTANCE instance)
{
    Q_UNUSED(parent);
    Q_UNUSED(instance);

    return m_hostWindow;
}


void Presentation::ScreenViewControl::resizeEvent(QResizeEvent *ev)
{
    // skip the window host, but chain up to qwidget right away
    QWidget::resizeEvent(ev);

    m_invalidateTimer->stop();
    m_resizeTimer->stop();
    m_resizeTimer->start(1000);

    ::ShowWindow(m_hostWindow, SW_HIDE);
}

void Presentation::ScreenViewControl::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);

    m_invalidateTimer->start();
    ::ShowWindow(m_hostWindow, SW_SHOWNOACTIVATE);
}

void Presentation::ScreenViewControl::hideEvent(QHideEvent *e)
{
    QWidget::hideEvent(e);

    m_invalidateTimer->stop();
    ::ShowWindow(m_hostWindow, SW_HIDE);
}

void Presentation::ScreenViewControl::focusInEvent(QFocusEvent *ev)
{
    QWidget::focusInEvent(ev);
}


bool Presentation::ScreenViewControl::nativeEvent(const QByteArray &eventType, void *message, long *result)
{
    return QWidget::nativeEvent(eventType, message, result);
}
