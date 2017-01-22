#ifndef ERRORWIDGET_H
#define ERRORWIDGET_H

#include <QWidget>

class QPropertyAnimation;
class QLabel;

namespace Recording {

class ErrorWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ErrorWidget(QWidget *parent = 0);

signals:

public slots:
    void displayError(const QString &message);
    void displayTemporaryWarning(const QString &message);
    void displayWarning(const QString &message);
    void displayNotice(const QString &message);
    void clearError();

private slots:
    void displayMessage(const QString &bgcolor, const QString &title, const QString &message);

private:
    QTimer *temporary_error_timer;
    QPropertyAnimation *max_height_anim;

    QLabel *status_icon_lbl;
    QLabel *title_message_lbl;
    QLabel *detailed_message_lbl;

    // QWidget interface
protected:
    void paintEvent(QPaintEvent *);
};

} // namespace Recording

#endif // ERRORWIDGET_H
