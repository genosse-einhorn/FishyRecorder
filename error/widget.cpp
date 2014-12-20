#include "error/widget.h"

#include <QLabel>
#include <QTimer>
#include <QGridLayout>
#include <QStyleOption>
#include <QPainter>
#include <QPropertyAnimation>
#include <QDebug>

Error::Widget::Widget(QWidget *parent) :
    QWidget(parent),
    temporary_error_timer(new QTimer(this)),
    max_height_anim(new QPropertyAnimation(this, "maximumHeight", this)),
    status_icon_lbl(new QLabel(this)),
    title_message_lbl(new QLabel(this)),
    detailed_message_lbl(new QLabel(this))
{
    QGridLayout *layout = new QGridLayout(this);
    layout->addWidget(status_icon_lbl, 0, 0, 2, 1, Qt::AlignHCenter | Qt::AlignTop);
    layout->addWidget(title_message_lbl, 0, 1);
    layout->addWidget(detailed_message_lbl, 1, 1);
    layout->setColumnStretch(1, 1);

    title_message_lbl->setTextFormat(Qt::RichText);
    title_message_lbl->setWordWrap(true); // will also force height-for-width layout for us
    detailed_message_lbl->setTextFormat(Qt::RichText);
    detailed_message_lbl->setWordWrap(true);

    max_height_anim->setEasingCurve(QEasingCurve::InOutExpo);

    // WHY? If any widget with size 0 exists, and we take the winId() of any (other) widget in the window
    // and the user moves the window to another screen, the app will crash with a failed QT_ASSERT.
    // QTBUG-43489
    this->setMaximumHeight(1);

    QObject::connect(temporary_error_timer, &QTimer::timeout, this, &Error::Widget::clearError);
    QObject::connect(max_height_anim, &QAbstractAnimation::finished, this, &Error::Widget::animationFinished);
}

void
Error::Widget::displayError(Error::Provider::ErrorType type, const QString &message1, const QString &message2)
{
    title_message_lbl->setText(QString("<b>%1</b>").arg(message1));
    detailed_message_lbl->setText(message2);

    temporary_error_timer->stop();

    switch (type) {
    case Error::Provider::ErrorType::NoError:
        clearError();
        return;
    case Error::Provider::ErrorType::TemporaryWarning:
        temporary_error_timer->setInterval(5000);
        temporary_error_timer->setSingleShot(true);
        temporary_error_timer->start();
    case Error::Provider::ErrorType::Warning:
        this->setStyleSheet("background-color: #f57900; color: white;");
        break;
    case Error::Provider::ErrorType::Notice:
        this->setStyleSheet("background-color: #4a90d9; color: white;");
        break;
    case Error::Provider::ErrorType::Error:
        this->setStyleSheet("background-color: #cc0000; color: white;");
        //TODO: set icon
        break;
    }

    max_height_anim->stop();
    max_height_anim->setStartValue(this->maximumHeight());
    max_height_anim->setEndValue(this->heightForWidth(this->width()));
    max_height_anim->setDuration(500);
    max_height_anim->start();
}

void
Error::Widget::clearError()
{
    temporary_error_timer->stop();
    max_height_anim->stop();
    max_height_anim->setStartValue(this->height());
    max_height_anim->setEndValue(1);
    max_height_anim->setDuration(500);
    max_height_anim->start();

}

void Error::Widget::animationFinished()
{
    if (max_height_anim->endValue() == 1) {
        this->setStyleSheet("background-color: transparent; color: transparent;");
    }
}

void Error::Widget::paintEvent(QPaintEvent *event)
{
    QStyleOption opt;
    opt.init(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);

    QWidget::paintEvent(event);
}
