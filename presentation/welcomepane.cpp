#include "welcomepane.h"
#include "ui_welcomepane.h"

#include <algorithm>
#include <QSettings>
#include <QScreen>
#include <QGuiApplication>

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
    QObject::connect(ui->openVideoButton, &QAbstractButton::clicked, this, &WelcomePane::videoRequested);

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

    ui->cbScreen->addItem(tr("< No Screen >"), QRect(0, 0, 0, 0));
    for (QScreen* screen : QGuiApplication::screens()) {
        ui->cbScreen->addItem(
                    QString("%1 [%2x%3 at %4,%5]")
                        .arg(screen->name())
                        .arg(screen->size().width())
                        .arg(screen->size().height())
                        .arg(screen->geometry().left())
                        .arg(screen->geometry().top()),
                    screen->geometry());
    }

    QObject::connect(ui->cbScreen, &QComboBox::currentTextChanged, this, &WelcomePane::cbScreenChanged);
    QObject::connect((QGuiApplication*)QGuiApplication::instance(), &QGuiApplication::screenAdded, this, &WelcomePane::screenAdded);
    QObject::connect((QGuiApplication*)QGuiApplication::instance(), &QGuiApplication::screenRemoved, this, &WelcomePane::screenRemoved);

    cbScreenChanged();
}

WelcomePane::~WelcomePane()
{
    delete ui;
}

void WelcomePane::setPresentationScreen(const QRect &screen)
{
    for (int i = 0; i < ui->cbScreen->count(); ++i) {
        if (ui->cbScreen->itemData(i).toRect() != screen)
            continue;

        ui->cbScreen->setCurrentIndex(i);
        break;
    }
}

void WelcomePane::cbScreenChanged()
{
    QRect screen = ui->cbScreen->currentData().toRect();

    emit presentationScreenChanged(screen);
}

void WelcomePane::screenAdded(QScreen *screen)
{
    ui->cbScreen->addItem(
                QString("%1 [%2x%3 at %4,%5]")
                    .arg(screen->name())
                    .arg(screen->size().width())
                    .arg(screen->size().height())
                    .arg(screen->geometry().left())
                    .arg(screen->geometry().top()),
                screen->geometry());
}

void WelcomePane::screenRemoved(QScreen *screen)
{
    for (int i = 0; i < ui->cbScreen->count(); ++i) {
        if (ui->cbScreen->itemData(i).toRect() == screen->geometry())
            ui->cbScreen->removeItem(i);
    }
}

} // namespace Presentation
