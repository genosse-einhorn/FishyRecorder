#include "presentationwindow.h"

#include <QStackedWidget>
#include <QGridLayout>
#include <QLabel>
#include <QApplication>
#include <QDesktopWidget>

#ifdef Q_OS_WIN32
#   include <QtWin>
#endif

namespace Presentation {

PresentationWindow::PresentationWindow(QWidget *parent) : QWidget(parent)
{
    this->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
    this->setAttribute(Qt::WA_ShowWithoutActivating);
    this->setWindowState(Qt::WindowFullScreen);
    this->setCursor(Qt::BlankCursor);

    m_stack = new QStackedWidget(this);
    m_stack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QGridLayout *gridLayout = new QGridLayout(this);
    gridLayout->setMargin(0);
    gridLayout->addWidget(m_stack, 0, 0, 1, 1, Qt::AlignCenter);

    this->setLayout(gridLayout);

    m_freezeLbl = new QLabel();
    m_freezeLbl->setAttribute(Qt::WA_ShowWithoutActivating);
    m_freezeLbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_stack->addWidget(m_freezeLbl);

    m_blankLbl = new QLabel();
    QPalette pal(m_blankLbl->palette());
    pal.setColor(QPalette::Background, Qt::black);
    m_blankLbl->setAutoFillBackground(true);
    m_blankLbl->setPalette(pal);
    m_blankLbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_blankLbl->setAttribute(Qt::WA_ShowWithoutActivating);
    m_stack->addWidget(m_blankLbl);

    m_stack->setCurrentWidget(m_blankLbl);

#ifdef Q_OS_WIN32
    QtWin::setWindowExcludedFromPeek(this, true);
#endif
}

PresentationWindow::~PresentationWindow()
{
    if (m_widget) {
        m_widget->setParent(nullptr);
    }
}

void PresentationWindow::setBlank(bool isBlank)
{
    if (m_isBlank == isBlank)
        return;

    m_isBlank = isBlank;
    updateStack();
    emit blankChanged(isBlank);
}

void PresentationWindow::setFreeze(bool isFreeze)
{
    if (m_isFreeze == isFreeze)
        return;

    m_isFreeze = isFreeze;
    if (m_isFreeze && !m_screen.isEmpty()) {
        // if we are just freezing, update the frozen image
        m_freezeLbl->setPixmap(QPixmap::grabWindow(QApplication::desktop()->winId(),
                                                   m_screen.x(),
                                                   m_screen.y(),
                                                   m_screen.width(),
                                                   m_screen.height()));
    }
    updateStack();
    emit freezeChanged(isFreeze);
}

void PresentationWindow::setScreen(const QRect &screen)
{
    m_screen = screen;
    if (!screen.isEmpty()) {
        this->setGeometry(screen);

        m_blankLbl->setFixedSize(screen.width(), screen.height());
    }

    updateStack();
}

void PresentationWindow::setWidget(QWidget *presentation)
{
    if ((presentation == m_widget) || !presentation)
        return;

    if (m_widget) {
        m_stack->removeWidget(m_widget);
        m_widget->setParent(nullptr);
        QObject::disconnect(m_widget, nullptr, this, nullptr);
    }

    m_widget = presentation;
    m_widget->setParent(this);
    m_widget->setAttribute(Qt::WA_ShowWithoutActivating);
    m_stack->addWidget(m_widget);
    QObject::connect(m_widget, &QObject::destroyed, this, &PresentationWindow::handleWidgetDestroy);

    updateStack();
}

void PresentationWindow::handleWidgetDestroy()
{
    // When we receive the event, the guarded pointer is not cleared yet.
    // So we do it manually so that updateStack() is happy.
    if (QObject::sender() == m_widget)
        m_widget = nullptr;

    updateStack();
}

void PresentationWindow::updateStack()
{
    if (!m_screen.isEmpty() && isBlank()) {
        m_stack->setCurrentWidget(m_blankLbl);
        this->setVisible(true);
    } else if (!m_screen.isEmpty() && isFreeze()) {
        m_stack->setCurrentWidget(m_freezeLbl);
        this->setVisible(true);
    } else if (!m_screen.isEmpty() && !m_widget.isNull()) {
        m_stack->setCurrentWidget(m_widget);
        this->setVisible(true);
    } else {
        this->hide();
    }

    m_stack->currentWidget()->setVisible(true);
}

} // namespace Presentation
