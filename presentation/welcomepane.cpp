#include "welcomepane.h"
#include "ui_welcomepane.h"

namespace Presentation {

WelcomePane::WelcomePane(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::WelcomePane)
{
    ui->setupUi(this);

    QObject::connect(ui->openPdfButton, &QAbstractButton::clicked, this, &WelcomePane::pdfRequested);
    QObject::connect(ui->openPptButton, &QAbstractButton::clicked, this, &WelcomePane::pptRequested);
}

WelcomePane::~WelcomePane()
{
    delete ui;
}

} // namespace Presentation
