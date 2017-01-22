#include "errorwidget.h"

#include <QLabel>
#include <QTimer>
#include <QGridLayout>
#include <QStyleOption>
#include <QPainter>
#include <QPropertyAnimation>
#include <QDebug>

namespace Recording {

ErrorWidget::ErrorWidget(QWidget *parent) :
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

    this->setMaximumHeight(0);

    QObject::connect(temporary_error_timer, &QTimer::timeout, this, &ErrorWidget::clearError);
}

void ErrorWidget::displayError(const QString &message)
{
    displayMessage("#cc0000", tr("Error"), message);
}

void ErrorWidget::displayTemporaryWarning(const QString &message)
{
    if (!message.size())
    {
        clearError();
    }
    else
    {
        displayWarning(message);

        temporary_error_timer->setInterval(5000);
        temporary_error_timer->setSingleShot(true);
        temporary_error_timer->start();
    }
}

void ErrorWidget::displayWarning(const QString &message)
{
    displayMessage("#f57900", tr("Warning"), message);
}

void ErrorWidget::displayNotice(const QString &message)
{
    displayMessage("#4a90d9", tr("Notice"), message);
}

void
ErrorWidget::displayMessage(const QString &bgcolor, const QString &title, const QString &message)
{
    if (message.size())
    {
        title_message_lbl->setText(QString("<b>%1</b>").arg(title));
        detailed_message_lbl->setText(message);

        temporary_error_timer->stop();

        this->setStyleSheet(QString("background-color: %1; color: white;").arg(bgcolor));
        //TODO: set icon

        max_height_anim->stop();
        max_height_anim->setStartValue(this->maximumHeight());
        max_height_anim->setEndValue(this->heightForWidth(this->width()));
        max_height_anim->setDuration(500);
        max_height_anim->start();
    }
    else
    {
        clearError();
    }
}

void
ErrorWidget::clearError()
{
    temporary_error_timer->stop();
    max_height_anim->stop();
    max_height_anim->setStartValue(this->height());
    max_height_anim->setEndValue(0);
    max_height_anim->setDuration(500);
    max_height_anim->start();
}

void ErrorWidget::paintEvent(QPaintEvent *event)
{
    QStyleOption opt;
    opt.init(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);

    QWidget::paintEvent(event);
}

} // namespace Recording
