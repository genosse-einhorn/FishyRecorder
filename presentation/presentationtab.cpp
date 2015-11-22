#include "presentationtab.h"
#include "ui_presentationtab.h"
#include "presentation/screenviewcontrol.h"
#include "presentation/welcomepane.h"
#include "presentation/pdfpresenter.h"
#include "util/misc.h"

#include <QGridLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>
#include <QLabel>
#include <QDesktopWidget>
#include <QAbstractNativeEventFilter>
#include <QAbstractEventDispatcher>

#ifdef Q_OS_WIN32
# include <QtWinExtras>
# include <QAxObject>
# include <windows.h>
#endif

namespace Presentation {

PresentationTab::PresentationTab(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PresentationTab),
    m_overlayWindow(new QLabel(this))
{
    ui->setupUi(this);

    m_welcome = new WelcomePane(this);
    ui->controlStack->insertWidget(0, m_welcome);

    QObject::connect(m_welcome, &WelcomePane::pdfRequested, this, &PresentationTab::openPdf);
    QObject::connect(m_welcome, &WelcomePane::pptRequested, this, &PresentationTab::openPpt);

    QObject::connect(ui->presentationTabWidget, &QTabWidget::currentChanged, this, &PresentationTab::tabChanged);

    m_overlayWindow->setWindowFlags(Qt::Window | Qt::Tool | Qt::FramelessWindowHint);
    m_overlayWindow->setAttribute(Qt::WA_ShowWithoutActivating);
    m_overlayWindow->setStyleSheet("background-color: black");
    m_overlayWindow->setAutoFillBackground(true);
    m_overlayWindow->setCursor(Qt::BlankCursor);

#ifdef Q_OS_WIN32
    QtWin::setWindowExcludedFromPeek(m_overlayWindow, true);
#endif
}

PresentationTab::~PresentationTab()
{
    delete ui;
}

void PresentationTab::screenUpdated(const QRect &screen)
{
#ifdef Q_OS_WIN32
    if (m_screenView)
        m_screenView->screenUpdated(screen);
#endif

    // reposition the blank window
    m_overlayWindow->setGeometry(screen.x(), screen.y(), screen.width(), screen.height());

#ifdef Q_OS_WIN32
    HWND hwnd = (HWND)m_overlayWindow->winId();
    ::SetWindowPos(hwnd, HWND_TOPMOST, screen.x(), screen.y(), screen.width(), screen.height(), SWP_NOACTIVATE);
#endif

    emit sigScreenChange(screen);

    m_currentScreen = screen;
}

void PresentationTab::blank(bool blank)
{
    if (!blank)
        return clear();

    Util::BooleanFlagSetter raiiFlag(&this->m_whileSettingOverlay);

    QPixmap p(m_currentScreen.width(), m_currentScreen.height());
    p.fill(Qt::black);
    m_overlayWindow->setPixmap(p);
    m_overlayWindow->show();

    emit freezeChanged(false);
    emit blankChanged(true);
}

void PresentationTab::freeze(bool freeze)
{
    if (!freeze)
        return clear();

    Util::BooleanFlagSetter raiiFlag(&this->m_whileSettingOverlay);

    m_overlayWindow->setPixmap(QPixmap::grabWindow(QApplication::desktop()->winId(),
                                                   m_currentScreen.x(),
                                                   m_currentScreen.y(),
                                                   m_currentScreen.width(),
                                                   m_currentScreen.height()));
    m_overlayWindow->show();

    emit blankChanged(false);
    emit freezeChanged(true);
}

void PresentationTab::clear()
{
    if (m_whileSettingOverlay)
        return;

    m_overlayWindow->hide();

    emit freezeChanged(false);
    emit blankChanged(false);
}

PdfPresenter *PresentationTab::doPresentPdf(const QString &filename)
{
    PdfPresenter *presenter = PdfPresenter::loadPdfFile(filename);


    if (!presenter) {
        QMessageBox::critical(this, tr("Could not open PDF"), tr("Unknown error occured"));

        return nullptr;
    }

    int index = ui->controlStack->insertWidget(-1, presenter);
    ui->controlStack->setCurrentIndex(index);

    QObject::connect(presenter, &PdfPresenter::closeRequested, presenter, &QObject::deleteLater);
    QObject::connect(presenter, &PdfPresenter::closeRequested, this, &PresentationTab::slotNoSlides);

    QObject::connect(this, &PresentationTab::sigNextSlide, presenter, &PdfPresenter::nextPage);
    QObject::connect(this, &PresentationTab::sigPreviousSlide, presenter, &PdfPresenter::previousPage);
    QObject::connect(this, &PresentationTab::sigScreenChange, presenter, &PdfPresenter::setScreen);

    QObject::connect(presenter, &PdfPresenter::canNextPageChanged, this, &PresentationTab::canNextSlideChanged);
    QObject::connect(presenter, &PdfPresenter::canPrevPageChanged, this, &PresentationTab::canPrevSlideChanged);

    presenter->setScreen(m_currentScreen);

    // initially sync nex/prev button activations
    emit canNextSlideChanged(presenter->canNextPage());
    emit canPrevSlideChanged(presenter->canPrevPage());

    return presenter;
}

void PresentationTab::openPdf()
{
    QString filename = QFileDialog::getOpenFileName(this, tr("Open PDF"),
                                                    QString(),
                                                    tr("PDF files (*.pdf)"));

    if (!filename.size())
        return;

    doPresentPdf(filename);
}

void PresentationTab::openPpt()
{
#ifdef Q_OS_WIN32
    QString filename = QFileDialog::getOpenFileName(this, tr("Open PPT"),
                                                    QString(),
                                                    tr("PowerPoint Presentations (*.ppt *.pptx)"));

    if (!filename.size())
        return;

    // Convert the PPT to a PDF file and use that
    // PowerPoint hasn't been very reliable for us, while the PDF viewer has proven
    // to be rock solid. PowerPoint can export to PDF since at least Office 2007 SP2
    QTemporaryDir *pdfdir = new QTemporaryDir();
    QString pdffile = QString("%1/a.pdf").arg(pdfdir->path());
    PdfPresenter *presenter = nullptr;

    QAxObject powerpoint("PowerPoint.Application");
    QAxObject *presentations, *presentation;
    if (powerpoint.isNull())
        goto error;

    presentations = powerpoint.querySubObject("Presentations");
    if (!presentations)
        goto error;

    presentation = presentations->querySubObject("Open(const QString&, int, int, int)", QDir::toNativeSeparators(filename), -1, -1, 0);
    if (!presentation)
        goto error;

    presentation->dynamicCall("SaveAs(const QString&, int)", QDir::toNativeSeparators(pdffile), 32);
    presentation->dynamicCall("Close()");

    presenter = doPresentPdf(pdffile);
    if (presenter)
        QObject::connect(presenter, &QObject::destroyed, [=](){
            delete pdfdir;
        });
    else
        delete pdfdir;

    return;
error:
    QMessageBox::critical(this, tr("Could not launch PPT"), tr("The PPT file could not be loaded."));
    delete pdfdir;
#endif /* Q_OS_WIN32 */
}

void PresentationTab::tabChanged()
{
#ifdef Q_OS_WIN32
    if (ui->presentationTabWidget->currentWidget() == ui->controlTab) {
        delete m_screenView;
        m_screenView = nullptr;
    } else if (!m_screenView) {
        m_screenView = new ScreenViewControl();
        int index = ui->screenViewStack->insertWidget(-1, m_screenView);
        ui->screenViewStack->setCurrentIndex(index);
        m_screenView->screenUpdated(m_currentScreen);
    }
#endif
}

} // namespace Presentation
