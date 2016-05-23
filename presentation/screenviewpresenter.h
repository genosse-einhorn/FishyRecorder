#ifndef PRESENTATION_SCREENVIEWPRESENTER_H
#define PRESENTATION_SCREENVIEWPRESENTER_H

#include "presentation/presenterbase.h"

#include <memory>

namespace Presentation {

class ScreenViewControl;

class ScreenViewPresenter : public PresenterBase
{
public:
    ScreenViewPresenter();
    ~ScreenViewPresenter();

private:
    std::unique_ptr<ScreenViewControl> m_screenView;
    QRect m_screen;

    // PresenterBase interface
public slots:
    void setScreen(const QRect &screen) override;
    void tabHidden() override;
    void tabVisible() override;

    // PresenterBase interface
public:
    QString title() override;
    bool allowClose() override { return false; }
};

} // namespace Presentation

#endif // PRESENTATION_SCREENVIEWPRESENTER_H
