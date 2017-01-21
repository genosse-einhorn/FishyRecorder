#ifndef PRESENTATION_WELCOMEPANE_H
#define PRESENTATION_WELCOMEPANE_H

#include <QWidget>

namespace Presentation {

namespace Ui {
class WelcomePane;
}

class WelcomePane : public QWidget
{
    Q_OBJECT

public:
    explicit WelcomePane(QWidget *parent = 0);
    ~WelcomePane();

signals:
    void pdfRequested();
    void pptRequested();
    void videoRequested();

    void presentationScreenChanged(const QRect& virtualCoordinates);

public slots:
    void setPresentationScreen(const QRect &coordinates);

private slots:
    void cbScreenChanged();
    void screenAdded(QScreen *screen);
    void screenRemoved(QScreen *screen);

private:
    Ui::WelcomePane *ui;
};


} // namespace Presentation
#endif // PRESENTATION_WELCOMEPANE_H
