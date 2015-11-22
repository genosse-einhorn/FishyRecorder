#include "welcomepane.h"
#include "ui_welcomepane.h"

#include <algorithm>

#ifdef Q_OS_WIN32
// For checking PowerPoint availability
# include <windows.h>
#endif

namespace Presentation {

namespace {
    void forceSquare(QWidget *widget)
    {
        if (!widget)
            return;

        int minW = widget->width();
        int minH = widget->height();
        int minS = std::max(minW, minH);
        widget->setMinimumSize(minS, minS);
    }
}

WelcomePane::WelcomePane(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::WelcomePane)
{
    ui->setupUi(this);

    QObject::connect(ui->openPdfButton, &QAbstractButton::clicked, this, &WelcomePane::pdfRequested);
    QObject::connect(ui->openPptButton, &QAbstractButton::clicked, this, &WelcomePane::pptRequested);

    // force square button appearance
    for (QObject *o : children())
        forceSquare(qobject_cast<QWidget*>(o));

    // Check for powerpoint
#ifdef Q_OS_WIN32
    CLSID dummy;
    ui->openPptButton->setEnabled(SUCCEEDED(CLSIDFromProgID(L"PowerPoint.Application", &dummy)));
#else
    ui->openPptButton->setEnabled(false);
#endif

    // Add ScreenView control

}

WelcomePane::~WelcomePane()
{
    delete ui;
}

} // namespace Presentation
