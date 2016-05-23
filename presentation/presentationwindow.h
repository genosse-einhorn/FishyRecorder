#ifndef PRESENTATION_PRESENTATIONWINDOW_H
#define PRESENTATION_PRESENTATIONWINDOW_H

#include <QWidget>
#include <QPointer>

class QStackedWidget;
class QLabel;

namespace Presentation {

class PresentationWindow : public QWidget
{
    Q_OBJECT
public:
    explicit PresentationWindow(QWidget *parent = 0);
    ~PresentationWindow();

    bool isBlank() const { return m_isBlank; }
    bool isFreeze() const { return m_isFreeze; }

signals:
    void blankChanged(bool isBlank);
    void freezeChanged(bool isFreeze);

public slots:
    void setBlank(bool isBlank = true);
    void setFreeze(bool isFreeze = true);
    void setScreen(const QRect &screen);

    /*
     * A presentation controller calls this method whenever it wants to
     * present something onto the screen.
     *
     * The widget will be reparented into the presentation window,
     * but logical ownership retains with the callee: The widget might be
     * unparented at any time, and will not be destroyed when the
     * presentation window is closed.
     */
    void setWidget(QWidget *presentation);

private slots:
    void handleWidgetDestroy();

private:
    QRect m_screen {};
    bool m_isBlank { false };
    bool m_isFreeze { false };
    QPointer<QWidget> m_widget;
    QLabel *m_freezeLbl { nullptr };
    QLabel *m_blankLbl { nullptr };
    QStackedWidget *m_stack { nullptr };

    void updateStack();

    // QWidget interface
protected:
    void closeEvent(QCloseEvent *) override;
};

} // namespace Presentation

#endif // PRESENTATION_PRESENTATIONWINDOW_H
