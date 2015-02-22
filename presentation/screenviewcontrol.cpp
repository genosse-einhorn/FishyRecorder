#include "screenviewcontrol.h"

#include "util/misc.h"

#include <windows.h>
#include <QLibrary>
#include <QDebug>
#include <QGridLayout>
#include <QLabel>
#include <QTimer>
#include <QDir>
#include <QTemporaryFile>
#include <algorithm>

namespace {
class screenview_t {
    typedef void (__cdecl *SV_LogHandler_t)(const char *message, void *userdata);

    void (__cdecl *SV_SetLogHandler)(SV_LogHandler_t handler, void *userdata);
    HWND (__cdecl *SV_CreateView)(HWND parent, int x, int y, int w, int h);
    void (__cdecl *SV_ChangeScreen)(HWND view, int x, int y, int w, int h);

    QFile dll;
    QLibrary lib;

public:
    screenview_t() : dll(QDir::temp().absoluteFilePath("screenview-x86.dll"))
    {
        // Extract DLL
        if (!dll.open(QFile::WriteOnly)) {
            qWarning() << "Failed to open dll file:" << dll.errorString();
            return;
        }

        QFile original(":/screenview-x86.dll");
        if (!original.open(QIODevice::ReadOnly)) {
            qWarning() << "Failed to open original dll file:" << original.errorString();
            return;
        }

        while (!original.atEnd()) {
            char buf[1024];
            qint64 read = original.read(buf, sizeof(buf));
            dll.write(buf, read);
        }

        dll.close();

        // Load it
        lib.setFileName(dll.fileName());
        if (!lib.load()) {
            qWarning() << "Failed to load library" << lib.errorString();
            return;
        }

        // Resolve our stuff
#define RESOLVE(name) this->name = (decltype(this->name))lib.resolve(#name)
        RESOLVE(SV_SetLogHandler);
        RESOLVE(SV_CreateView);
        RESOLVE(SV_ChangeScreen);
#undef RESOLVE

        if (!SV_SetLogHandler || !SV_CreateView || !SV_ChangeScreen) {
            qWarning() << "Failed to load functions from the screenview dll";
            return;
        }

        SV_SetLogHandler(&OurLogHandler, nullptr);
    }

    ~screenview_t()
    {
        // This is not as neat as it could be because the file stays around for an indefinite time,
        // but it's the best we can get without doing complicated work.
        MoveFileEx((const wchar_t*)dll.fileName().utf16(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
    }

    HWND createView(HWND parent, int x, int y, int w, int h)
    {
        if (!SV_CreateView) {
            qWarning() << "screenview dll not present :(";
            return 0;
        }

        return SV_CreateView(parent, x, y, w, h);
    }

    void changeScreen(HWND view, int x, int y, int w, int h)
    {
        if (!SV_ChangeScreen) {
            qWarning() << "screenview dll not present :(";
            return;
        }

        SV_ChangeScreen(view, x, y, w, h);
    }

private:
    static __cdecl void OurLogHandler(const char *message, void*)
    {
        qDebug() << "SCREENVIEW:" << message;
    }

};

    static screenview_t screenview;
}

namespace Presentation {

ScreenViewControl::ScreenViewControl(QWidget *parent) :
    QWinHost(parent)
{
}

ScreenViewControl::~ScreenViewControl()
{

}

void ScreenViewControl::screenUpdated(const QRect &rect)
{
    m_screen = rect;

    qDebug() << "Screen has been set to" << rect;

    this->updateGeometry();

    if (window())
        screenview.changeScreen(window(), rect.x(), rect.y(), rect.width(), rect.height());
}

void ScreenViewControl::setExcludedWindow(HWND)
{
    qWarning() << "Excluding not supported :(";
}

QSize ScreenViewControl::getViewWindowSize()
{
    if (!m_screen.width() | !m_screen.height())
        return QSize(width(), height());

    double available_width = static_cast<double>(width());
    double available_height = static_cast<double>(height());
    double screen_width = static_cast<double>(m_screen.width());
    double screen_height = static_cast<double>(m_screen.height());

    double factor_x = available_width/screen_width;
    double factor_y = available_height/screen_height;

    double factor = std::min(factor_x, factor_y);

    return QSize(static_cast<int>(screen_width * factor), static_cast<int>(screen_height * factor));
}

} // namespace Presentation

HWND Presentation::ScreenViewControl::createWindow(HWND parent, HINSTANCE instance)
{
    Q_UNUSED(instance);

    return screenview.createView(parent, m_screen.x(), m_screen.y(), m_screen.width(), m_screen.height());
}

void Presentation::ScreenViewControl::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);

    if (window()) {
        QSize sz = getViewWindowSize();
        SetWindowPos(window(), HWND_TOP, (width()-sz.width())/2, (height()-sz.height())/2, sz.width(), sz.height(), 0);
    }
}

void Presentation::ScreenViewControl::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);

    if (window()) {
        QSize sz = getViewWindowSize();
        SetWindowPos(window(), HWND_TOP, (width()-sz.width())/2, (height()-sz.height())/2, sz.width(), sz.height(), SWP_SHOWWINDOW);
    }
}
