#ifndef ERRORWIDGET_H
#define ERRORWIDGET_H

#include <QWidget>
#include "error/provider.h"

class QPropertyAnimation;
class QLabel;

namespace Error {

class Widget : public QWidget
{
    Q_OBJECT
public:
    explicit Widget(QWidget *parent = 0);

signals:

public slots:
    void displayError(Error::Provider::ErrorType type, const QString& message1, const QString& message2);
    void clearError();

private slots:
    void animationFinished();

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

} // namespace Error

#endif // ERRORWIDGET_H
