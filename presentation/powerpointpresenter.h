#ifndef PRESENTATION_POWERPOINTPRESENTER_H
#define PRESENTATION_POWERPOINTPRESENTER_H

#include <QWidget>
#include <memory>

class QAxScript;
class QAxScriptManager;

namespace Presentation {

namespace Ui {
class PowerpointPresenter;
}

class PowerpointPresenter : public QWidget
{
    Q_OBJECT

public:
    static PowerpointPresenter *loadPowerpointFile(const QString& fileName);
    ~PowerpointPresenter();

signals:
    void closeRequested();

public:
    bool canNextPage() { return true; }
    bool canPrevPage() { return true; }

public slots:
    void setScreen(const QRect& screen);
    void nextPage();
    void previousPage();

private slots:
    void closeBtnClicked();
    void itemSelected();
    void scriptError(QAxScript * script, int code, const QString & description, int sourcePosition, const QString & sourceText);

private:
    explicit PowerpointPresenter(QWidget *parent = 0);
    void fillPreviewList();
    void updateCurrentSlide();

    QAxScriptManager *m_scripts = nullptr;
    QAxScript        *m_script  = nullptr;
    bool              m_scriptError = false;

    Ui::PowerpointPresenter *ui;
};


} // namespace Presentation
#endif // PRESENTATION_POWERPOINTPRESENTER_H
