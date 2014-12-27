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
#include <QtWinExtras>

#include <windows.h>

namespace Presentation {

PresentationTab::PresentationTab(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PresentationTab),
    m_overlayWindow(new QLabel(this))
{
    ui->setupUi(this);

    m_welcome = new WelcomePane(this);
    ui->sidebar->insertWidget(0, m_welcome);

    QObject::connect(m_welcome, &WelcomePane::pdfRequested, this, &PresentationTab::openPdf);

    m_overlayWindow->setWindowFlags(Qt::Window | Qt::Tool | Qt::FramelessWindowHint);
    m_overlayWindow->setAttribute(Qt::WA_ShowWithoutActivating);
    m_overlayWindow->setStyleSheet("background-color: black");
    m_overlayWindow->setAutoFillBackground(true);
    m_overlayWindow->setCursor(Qt::BlankCursor);
    QtWin::setWindowExcludedFromPeek(m_overlayWindow, true);

    //FIXME: doesn't really seem to work, have to call again
    ui->screenView->setExcludedWindow((HWND)m_overlayWindow->winId());
}

PresentationTab::~PresentationTab()
{
    delete ui;
}

void PresentationTab::screenUpdated(const QRect &screen)
{
    ui->screenView->screenUpdated(screen);

    // reposition the blank window
    m_overlayWindow->setGeometry(screen.x(), screen.y(), screen.width(), screen.height());

    HWND hwnd = (HWND)m_overlayWindow->winId();
    ::SetWindowPos(hwnd, HWND_TOPMOST, screen.x(), screen.y(), screen.width(), screen.height(), SWP_NOACTIVATE);

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

    // ensure exclusion
    ui->screenView->setExcludedWindow((HWND)m_overlayWindow->winId());

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

    // ensure exclusion
    ui->screenView->setExcludedWindow((HWND)m_overlayWindow->winId());

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

void PresentationTab::openPdf()
{
    QString filename = QFileDialog::getOpenFileName(this, tr("Open PDF"),
                                                    QString(),
                                                    tr("PDF files (*.pdf)"));

    if (!filename.size())
        return;

    if (m_currentScreen.width() < 1) {
        QMessageBox::critical(this, tr("No presentation screen"), tr("Select a screen to present on, first"));

        return;
    }

    PdfPresenter *presenter = PdfPresenter::loadPdfFile(filename);


    if (!presenter) {
        QMessageBox::critical(this, tr("Could not open PDF"), tr("Unknown error occured"));

        return;
    }

    QObject::connect(presenter, &PdfPresenter::closeRequested, presenter, &QObject::deleteLater);
    QObject::connect(presenter, &PdfPresenter::closeRequested, this, &PresentationTab::slotNoSlides);

    QObject::connect(this, &PresentationTab::sigNextSlide, presenter, &PdfPresenter::nextPage);
    QObject::connect(this, &PresentationTab::sigPreviousSlide, presenter, &PdfPresenter::previousPage);
    QObject::connect(this, &PresentationTab::sigScreenChange, presenter, &PdfPresenter::setScreen);

    QObject::connect(presenter, &PdfPresenter::canNextPageChanged, this, &PresentationTab::slotCanNextSlideChanged);
    QObject::connect(presenter, &PdfPresenter::canPrevPageChanged, this, &PresentationTab::slotCanPrevSlideChanged);

    presenter->setScreen(m_currentScreen);

    int index = ui->sidebar->insertWidget(-1, presenter);
    ui->sidebar->setCurrentIndex(index);
}

void PresentationTab::slotCanNextSlideChanged(bool can)
{
    emit canNextSlideChanged(can);
}

void PresentationTab::slotCanPrevSlideChanged(bool can)
{
    emit canPrevSlideChanged(can);
}

} // namespace Presentation
