#ifndef PRESENTATION_PRESENTATIONTAB_H
#define PRESENTATION_PRESENTATIONTAB_H

#include <QWidget>

namespace Presentation {

namespace Ui {
class PresentationTab;
}

class PresentationTab : public QWidget
{
    Q_OBJECT

public:
    explicit PresentationTab(QWidget *parent = 0);
    ~PresentationTab();

public slots:
    void screenUpdated(const QRect& screen);

private:
    Ui::PresentationTab *ui;

};


} // namespace Presentation
#endif // PRESENTATION_PRESENTATIONTAB_H
