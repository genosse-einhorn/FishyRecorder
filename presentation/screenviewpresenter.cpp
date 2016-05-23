#include "screenviewpresenter.h"

#include "presentation/screenviewcontrol.h"

#include <QVBoxLayout>

namespace Presentation {

ScreenViewPresenter::ScreenViewPresenter()
{    
    QVBoxLayout *l = new QVBoxLayout;
    l->setMargin(0);

    this->setLayout(l);
}

ScreenViewPresenter::~ScreenViewPresenter()
{
}

} // namespace Presentation


void Presentation::ScreenViewPresenter::setScreen(const QRect &screen)
{
    m_screen = screen;

    if (m_screenView)
        m_screenView->screenUpdated(screen);
}

void Presentation::ScreenViewPresenter::tabHidden()
{
    m_screenView.reset();
}

void Presentation::ScreenViewPresenter::tabVisible()
{
    m_screenView.reset(new ScreenViewControl);
    this->layout()->addWidget(m_screenView.get());
    m_screenView->screenUpdated(m_screen);
}


QString Presentation::ScreenViewPresenter::title()
{
    return tr("Screen View");
}
