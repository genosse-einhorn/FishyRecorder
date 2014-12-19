#include "presentationtab.h"
#include "ui_presentationtab.h"
#include "presentation/screenviewcontrol.h"

#include <QGridLayout>

namespace Presentation {

PresentationTab::PresentationTab(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PresentationTab)
{
    ui->setupUi(this);
}

PresentationTab::~PresentationTab()
{
    delete ui;
}

void PresentationTab::screenUpdated(const QRect &screen)
{
    ui->screenView->screenUpdated(screen);
}

} // namespace Presentation
