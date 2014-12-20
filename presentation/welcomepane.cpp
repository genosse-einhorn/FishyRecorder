#include "welcomepane.h"
#include "ui_welcomepane.h"

namespace Presentation {

WelcomePane::WelcomePane(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::WelcomePane)
{
    ui->setupUi(this);

    QObject::connect(ui->openPdfButton, &QAbstractButton::clicked, this, &WelcomePane::pdfButtonClicked);
}

WelcomePane::~WelcomePane()
{
    delete ui;
}

void WelcomePane::pdfButtonClicked()
{
    emit pdfRequested();
}

} // namespace Presentation
