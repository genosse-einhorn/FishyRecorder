#ifndef PRESENTATION_PRESENTATIONTAB_H
#define PRESENTATION_PRESENTATIONTAB_H

#include <QWidget>

class QLabel;

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

    void freezeChanged(bool freezing);
    void blankChanged(bool isBlank);

public slots:
    void screenUpdated(const QRect& screen);
    void previousSlide() { emit sigPreviousSlide(); }
    void nextSlide() { emit sigNextSlide(); }

    // screen blanking
    void blank(bool blank = true);
    void freeze(bool freeze = true);
    void clear();

private slots:
    void openPdf();

private:
    Ui::PresentationTab *ui;
    WelcomePane *m_welcome;

    QLabel  *m_overlayWindow;
    bool     m_whileSettingOverlay = false;

    QRect m_currentScreen { 0, 0, 0, 0 };
};


} // namespace Presentation
#endif // PRESENTATION_PRESENTATIONTAB_H
