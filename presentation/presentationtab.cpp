#include "presentationtab.h"
#include "ui_presentationtab.h"
#include "presentation/welcomepane.h"
#include "presentation/pdfpresenter.h"
#include "presentation/mediapresenter.h"
#include "presentation/presentationwindow.h"
#include "presentation/presenterbase.h"
#include "util/misc.h"

#include <QGridLayout>
#include <QTemporaryDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>
#include <QLabel>
#include <QSettings>

#ifdef Q_OS_WIN32
#   include <QAxObject>
#endif

namespace Presentation {

PresentationTab::PresentationTab(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PresentationTab)
{
    ui->setupUi(this);

    m_welcome = new WelcomePane(this);
    ui->controlStack->insertWidget(0, m_welcome);

    ui->presentationTabWidget->tabBar()->setTabButton(0, QTabBar::LeftSide, nullptr);
    ui->presentationTabWidget->tabBar()->setTabButton(0, QTabBar::RightSide, nullptr);

    QObject::connect(m_welcome, &WelcomePane::pdfRequested, this, &PresentationTab::openPdf);
    QObject::connect(m_welcome, &WelcomePane::pptRequested, this, &PresentationTab::openPpt);
    QObject::connect(m_welcome, &WelcomePane::videoRequested, this, &PresentationTab::openVideo);
    QObject::connect(m_welcome, &WelcomePane::presentationScreenChanged, this, &PresentationTab::screenUpdated);

    QObject::connect(ui->presentationTabWidget, &QTabWidget::currentChanged, this, &PresentationTab::tabChanged);
    QObject::connect(ui->presentationTabWidget, &QTabWidget::tabCloseRequested, this, &PresentationTab::tabClosed);

    m_presentationWindow = new PresentationWindow;
    QObject::connect(m_presentationWindow, &PresentationWindow::blankChanged, this, &PresentationTab::blankChanged);
    QObject::connect(m_presentationWindow, &PresentationWindow::freezeChanged, this, &PresentationTab::freezeChanged);

    screenUpdated(QSettings().value("Presentation Screen", QVariant::fromValue(QRect())).toRect());
}

PresentationTab::~PresentationTab()
{
    delete ui;
    delete m_presentationWindow;
}

void PresentationTab::screenUpdated(const QRect &screen)
{
    // save screen position
    m_welcome->setPresentationScreen(screen);
    QSettings().setValue("Presentation Screen", QVariant::fromValue(screen));

    // reposition the presentation window
    m_presentationWindow->setScreen(screen);

    emit sigScreenChange(screen);

    m_currentScreen = screen;
}

void PresentationTab::previousSlide()
{
    PresenterBase *p = qobject_cast<PresenterBase*>(ui->presentationTabWidget->currentWidget());
    if (p) {
        p->previousPage();
    }
}

void PresentationTab::nextSlide()
{
    PresenterBase *p = qobject_cast<PresenterBase*>(ui->presentationTabWidget->currentWidget());
    if (p) {
        p->nextPage();
    }
}

void PresentationTab::blank(bool blank)
{
    m_presentationWindow->setBlank(blank);
}

void PresentationTab::freeze(bool freeze)
{
    m_presentationWindow->setFreeze(freeze);
}

PdfPresenter *PresentationTab::doPresentPdf(const QString &filename, const QString &title)
{
    PdfPresenter *presenter = PdfPresenter::loadPdfFile(filename, title);


    if (!presenter) {
        QMessageBox::critical(this, tr("Could not open PDF"), tr("Unknown error occured"));

        return nullptr;
    }

    int i = insertTab(presenter);
    ui->presentationTabWidget->setCurrentIndex(i);

    return presenter;
}

int PresentationTab::insertTab(PresenterBase *widget)
{
    int tabIndex = ui->presentationTabWidget->insertTab(-1, widget, widget->title());

    QObject::connect(widget, &PresenterBase::requestPresentation, m_presentationWindow, &PresentationWindow::setWidget);
    QObject::connect(widget, &PresenterBase::controlsChanged, this, &PresentationTab::syncTabState);
    QObject::connect(this, &PresentationTab::sigScreenChange, widget, &PresenterBase::setScreen);

    widget->setScreen(m_currentScreen);

    if (!widget->allowClose()) {
        ui->presentationTabWidget->tabBar()->setTabButton(tabIndex, QTabBar::LeftSide, nullptr);
        ui->presentationTabWidget->tabBar()->setTabButton(tabIndex, QTabBar::RightSide, nullptr);
    }

    return tabIndex;
}

void PresentationTab::syncTabState()
{
    PresenterBase *p = qobject_cast<PresenterBase*>(ui->presentationTabWidget->currentWidget());
    if (!p)
        return; // nothing to do here

    ui->presentationTabWidget->setTabText(ui->presentationTabWidget->currentIndex(), p->title());
    emit canNextSlideChanged(p->canNextPage());
    emit canPrevSlideChanged(p->canPrevPage());
}

void PresentationTab::tabClosed(int index)
{
    auto w = ui->presentationTabWidget->widget(index);
    if (w)
        w->deleteLater();
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

    presenter = doPresentPdf(pdffile, QFileInfo(filename).fileName());
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

void PresentationTab::openVideo()
{
    QString filename = QFileDialog::getOpenFileName(this, tr("Open Media"),
                                                    QString(),
                                                    tr("All Files (*.*)"));

    if (!filename.size())
        return;

    MediaPresenter *p = new MediaPresenter(filename);

    int i = insertTab(p);
    ui->presentationTabWidget->setCurrentIndex(i);
}

void PresentationTab::tabChanged()
{
    // Notify last active tab about being hidden now
    if (m_lastActiveTab) {
        PresenterBase *p = qobject_cast<PresenterBase*>(m_lastActiveTab);
        if (p) {
            p->tabHidden();
        }
    }

    // Update our state variable
    m_lastActiveTab = ui->presentationTabWidget->currentWidget();

    // Notify the new tab about being shown
    if (ui->presentationTabWidget->currentWidget()) {
        PresenterBase *p = qobject_cast<PresenterBase*>(ui->presentationTabWidget->currentWidget());
        if (p) {
            p->tabVisible();
        }
    }

    // Update button states
    syncTabState();
}

} // namespace Presentation
