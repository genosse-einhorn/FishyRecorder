#include "presentationtab.h"
#include "ui_presentationtab.h"
#include "presentation/screenviewcontrol.h"
#include "presentation/welcomepane.h"
#include "presentation/pdfpresenter.h"

#include <QGridLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>

namespace Presentation {

PresentationTab::PresentationTab(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PresentationTab)
{
    ui->setupUi(this);

    m_welcome = new WelcomePane(this);
    ui->sidebar->insertWidget(0, m_welcome);

    QObject::connect(m_welcome, &WelcomePane::pdfRequested, this, &PresentationTab::openPdf);
}

PresentationTab::~PresentationTab()
{
    delete ui;
}

void PresentationTab::screenUpdated(const QRect &screen)
{
    ui->screenView->screenUpdated(screen);

    emit sigScreenChange(screen);

    m_currentScreen = screen;
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

    presenter->setScreen(m_currentScreen);
    QObject::connect(presenter, &PdfPresenter::closeRequested, presenter, &QObject::deleteLater);

    QObject::connect(this, &PresentationTab::sigNextSlide, presenter, &PdfPresenter::nextPage);
    QObject::connect(this, &PresentationTab::sigPreviousSlide, presenter, &PdfPresenter::previousPage);
    QObject::connect(this, &PresentationTab::sigScreenChange, presenter, &PdfPresenter::setScreen);

    int index = ui->sidebar->insertWidget(-1, presenter);
    ui->sidebar->setCurrentIndex(index);
}

} // namespace Presentation
