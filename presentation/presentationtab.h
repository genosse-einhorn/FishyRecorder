#ifndef PRESENTATION_PRESENTATIONTAB_H
#define PRESENTATION_PRESENTATIONTAB_H

#include <QWidget>
#include <QPointer>

class QLabel;

namespace Presentation {

namespace Ui {
class PresentationTab;
}

class WelcomePane;
class PdfPresenter;
class PresentationWindow;
class PresenterBase;

class PresentationTab : public QWidget
{
    Q_OBJECT

public:
    explicit PresentationTab(QWidget *parent = 0);
    ~PresentationTab();

signals:
    void sigScreenChange(const QRect& screen);
    void sigNextSlide();
    void sigPreviousSlide();

    void freezeChanged(bool freezing);
    void blankChanged(bool isBlank);
    void canNextSlideChanged(bool can);
    void canPrevSlideChanged(bool can);

public slots:
    void screenUpdated(const QRect& screen);
    void previousSlide();
    void nextSlide();

    // screen blanking
    void blank(bool blank = true);
    void freeze(bool freeze = true);

private slots:
    void openPdf();
    void openPpt();
    void slotNoSlides() { canNextSlideChanged(false); canPrevSlideChanged(false); }
    void tabChanged();
    void syncTabState();
    void tabClosed(int index);

private:
    Ui::PresentationTab *ui;
    WelcomePane *m_welcome;

    PresentationWindow *m_presentationWindow { nullptr };

    QRect m_currentScreen { 0, 0, 0, 0 };
    QPointer<QWidget> m_lastActiveTab;

    Presentation::PdfPresenter *doPresentPdf(const QString &filename);

    int insertTab(PresenterBase *widget);
};


} // namespace Presentation
#endif // PRESENTATION_PRESENTATIONTAB_H
