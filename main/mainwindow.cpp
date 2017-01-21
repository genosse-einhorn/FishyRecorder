#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "util/misc.h"
#include "main/aboutpane.h"
#include "presentation/presentationtab.h"

#include <QDebug>
#include <QFile>
#include <QCloseEvent>
#include <QThread>
#include <QCommonStyle>
#include <QShortcut>
#include <portaudio.h>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

#ifdef Q_OS_WIN32
    this->setAttribute(Qt::WA_TranslucentBackground); // WS_LAYERED
    this->setAttribute(Qt::WA_NoSystemBackground, false);
#endif

    //QObject::connect(ui->recordBtn, &QAbstractButton::toggled, m_mover, &Recording::SampleMover::setRecordingState);
    //QObject::connect(ui->monitorBtn, &QAbstractButton::toggled, m_mover, &Recording::SampleMover::setMonitorEnabled);

    // HACK: We really don't want the left/right keys to trigger something else when the button
    // is not enabled. That's why we are creating a manual shortcut.
    QObject::connect(new QShortcut(QKeySequence(Qt::Key_Right), this),
                     &QShortcut::activated, ui->nextBtn, &QAbstractButton::click);
    QObject::connect(new QShortcut(QKeySequence(Qt::Key_Down), this),
                     &QShortcut::activated, ui->nextBtn, &QAbstractButton::click);
    QObject::connect(new QShortcut(QKeySequence(Qt::Key_Left), this),
                     &QShortcut::activated, ui->prevBtn, &QAbstractButton::click);
    QObject::connect(new QShortcut(QKeySequence(Qt::Key_Up), this),
                     &QShortcut::activated, ui->prevBtn, &QAbstractButton::click);

    m_presentation = new Presentation::PresentationTab(this);

    QObject::connect(ui->nextBtn, &QAbstractButton::clicked, m_presentation, &Presentation::PresentationTab::nextSlide);
    QObject::connect(ui->prevBtn, &QAbstractButton::clicked, m_presentation, &Presentation::PresentationTab::previousSlide);
    QObject::connect(ui->freezeBtn, &QAbstractButton::toggled, m_presentation, &Presentation::PresentationTab::freeze);
    QObject::connect(ui->blankBtn, &QAbstractButton::toggled, m_presentation, &Presentation::PresentationTab::blank);
    QObject::connect(m_presentation, &Presentation::PresentationTab::freezeChanged, ui->freezeBtn, &QAbstractButton::setChecked);
    QObject::connect(m_presentation, &Presentation::PresentationTab::blankChanged, ui->blankBtn, &QAbstractButton::setChecked);
    QObject::connect(m_presentation, &Presentation::PresentationTab::canNextSlideChanged, ui->nextBtn, &QAbstractButton::setEnabled);
    QObject::connect(m_presentation, &Presentation::PresentationTab::canPrevSlideChanged, ui->prevBtn, &QAbstractButton::setEnabled);

    ui->freezeBtn->setEnabled(true);
    ui->blankBtn->setEnabled(true);

    ui->tabWidget->addTab(m_presentation, tr("Presentation"));

    ui->tabWidget->addTab(new AboutPane(this), tr("About"));

    // == Main styling ==
    QPalette pal = this->palette();
    pal.setBrush(QPalette::Background, QColor(0x30, 0x30, 0x30));
    pal.setBrush(QPalette::Foreground, QColor(0xFF, 0xFF, 0xFF));
    pal.setBrush(QPalette::Light, QColor(0x40, 0x40, 0x40));
    this->setPalette(pal);
    this->setAutoFillBackground(true);

    // == tab widget styling ==
    // We use a normal QTabWidget, but style the tabs according
    // to our "custom theme" using CSS
    ui->tabWidget->setPalette(QGuiApplication::palette());
    for (int i = 0; i < ui->tabWidget->count(); ++i) {
        QWidget *w = ui->tabWidget->widget(i);
        w->setAutoFillBackground(true);
        w->setPalette(w->palette());
        w->setBackgroundRole(QPalette::Base);
    }

    ui->tabWidget->setStyleSheet(QString(
        "#tabWidget > QTabBar::tab {"
        "   background-color: %1;"
        "   color: %2;"
        "   padding: 10px;"
        "   border: 0;"
        "   margin-left: 5px;"
        "}"
        "#tabWidget > QTabBar::tab:selected {"
        "   background-color: %3;"
        "   color: %4;"
        "   margin-left: 0;"
        "}"
        "#tabWidget::pane {"
        "  border: 0"
        "}"
    ).arg(this->palette().color(QPalette::Light).name())
     .arg(this->palette().color(QPalette::Foreground).name())
     .arg(ui->tabWidget->widget(0)->palette().color(ui->tabWidget->widget(0)->backgroundRole()).name())
     .arg(ui->tabWidget->widget(0)->palette().color(QPalette::Text).name()));

    // == bottom button styling ==
    // This can be easily done via style sheets, saving us from subclassing QPushButton
    ui->bottomButtonBox->setStyleSheet(QString(
        "QPushButton {"
        "   background-color: %3;"
        "   color: %2;"
        "   border: 1px solid %3;"
        "   outline: 0;"
        "   padding: 2px 10px;"
        "}"
        "QPushButton:!enabled {"
        "   background-color: %1;"
        "}"
        "QPushButton:pressed, QPushButton:checked {"
        "   background-color: %2;"
        "   color: %1;"
        "   border-color: %2;"
        "}"
        "QPushButton:hover {"
        "   border-color: %2;"
        "}"
        "QPushButton:checked:hover, QPushButton:pressed:hover {"
        "   border-color: %3;"
        "   background-color: %2;"
        "}"
    ).replace("%1", this->palette().color(QPalette::Window).name())
     .replace("%2", this->palette().color(QPalette::Foreground).name())
     .replace("%3", this->palette().color(QPalette::Light).name()));
}

MainWindow::~MainWindow()
{
}
