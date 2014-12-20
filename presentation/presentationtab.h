#ifndef PRESENTATION_PRESENTATIONTAB_H
#define PRESENTATION_PRESENTATIONTAB_H

#include <QWidget>

namespace Presentation {

namespace Ui {
class PresentationTab;
}

class WelcomePane;

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

public slots:
    void screenUpdated(const QRect& screen);
    void previousSlide() { emit sigPreviousSlide(); }
    void nextSlide() { emit sigNextSlide(); }

private slots:
    void openPdf();

private:
    Ui::PresentationTab *ui;
    WelcomePane *m_welcome;

    QRect m_currentScreen { 0, 0, 0, 0 };
};


} // namespace Presentation
#endif // PRESENTATION_PRESENTATIONTAB_H
