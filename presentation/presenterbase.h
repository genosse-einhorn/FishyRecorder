#ifndef PRESENTATION_PRESENTERBASE_H
#define PRESENTATION_PRESENTERBASE_H

#include <QWidget>

namespace Presentation {

// base class for a tab managing presentation of something, e.g. a PDF file.
//
// Anything that wants to create a tab in the presentation unit must inhert from this
// class. If the user closes the tab, the widget will be deleted. If you want to close
// a tab from inside, just deleteLater() yourself.
class PresenterBase : public QWidget
{
    Q_OBJECT
public:
    explicit PresenterBase(QWidget *parent = 0);

public:
    virtual bool canNextPage() { return false; }
    virtual bool canPrevPage() { return false; }
    virtual QString title() { return QString(); }
    virtual bool allowClose() { return true; }

public slots:
    virtual void setScreen(const QRect& /*screen*/) {}
    virtual void nextPage() {}
    virtual void previousPage() {}
    virtual void tabHidden() {}
    virtual void tabVisible() {}

signals:
    // supposed to be connected to PresentationWindow::presentWidget
    //
    // Emit this whenever you want to show something on the presentation screen.
    // Do it after every user interaction that involves changing the currently presented image.
    void requestPresentation(QWidget *widget);

    // emit this if can{Next, Prev}Page() changed
    void controlsChanged();
};

} // namespace Presentation

#endif // PRESENTATION_PRESENTERBASE_H
