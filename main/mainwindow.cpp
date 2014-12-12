#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "recording/samplemover.h"
#include "recording/trackcontroller.h"
#include "recording/trackviewpane.h"
#include "error/simulationwidget.h"
#include "config/database.h"
#include "util/misc.h"
#include "main/configpane.h"
#include "main/quitdialog.h"
#include "main/aboutpane.h"

#include <QDebug>
#include <QFile>
#include <QCloseEvent>
#include <QThread>
#include <portaudio.h>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_config(new Config::Database(this)),
    m_mover(new Recording::SampleMover(m_config->readTotalSamplesRecorded())),
    m_trackController(new Recording::TrackController(m_config, this)),
    m_trackPane(new Recording::TrackViewPane(m_trackController, this)),
    m_configPane(new ConfigPane(m_config, this)),
    m_quitDialog(new QuitDialog(this)),
    m_moverThread(new QThread())
{
    ui->setupUi(this);

    // setup the thread first
    m_mover->moveToThread(m_moverThread);
    QObject::connect(m_moverThread, &QThread::finished, m_mover, &QObject::deleteLater); // this is legal and even recommended by the docs for QThread::finished
    m_moverThread->start();

    QObject::connect(ui->recordBtn, &QAbstractButton::toggled, m_mover, &Recording::SampleMover::setRecordingState);
    QObject::connect(ui->trackBtn, &QAbstractButton::clicked, this, &MainWindow::trackBtnClicked);
    QObject::connect(ui->trackNextBtn, &QAbstractButton::clicked, this, &MainWindow::trackBtnClicked);
    QObject::connect(ui->monitorBtn, &QAbstractButton::toggled, m_mover, &Recording::SampleMover::setMonitorEnabled);

    QObject::connect(m_configPane, &ConfigPane::recordDeviceChanged, m_mover, &Recording::SampleMover::setInputDevice);
    QObject::connect(m_configPane, &ConfigPane::monitorDeviceChanged, m_mover, &Recording::SampleMover::setMonitorDevice);
    QObject::connect(m_configPane, &ConfigPane::monitorDeviceChanged, m_trackPane, &Recording::TrackViewPane::setMonitorDevice);
    QObject::connect(m_configPane, &ConfigPane::availableAudioSpaceChanged, this, &MainWindow::availableSpaceUpdate);
    QObject::connect(m_configPane, &ConfigPane::audioDataDirChanged, m_mover, &Recording::SampleMover::setAudioDataDir);

    QObject::connect(m_mover, &Recording::SampleMover::timeUpdate, this, &MainWindow::timeUpdate);
    QObject::connect(m_mover, &Recording::SampleMover::levelMeterUpdate, this, &MainWindow::levelMeterUpdate);
    QObject::connect(m_mover, &Recording::SampleMover::recordingStateChanged, ui->recordBtn, &QAbstractButton::setChecked);
    QObject::connect(m_mover->getDeviceErrorProvider(), &Error::Provider::error, m_configPane, &ConfigPane::displayDeviceError);
    QObject::connect(m_mover->getRecordingErrorProvider(), &Error::Provider::error, ui->recordingErrorDisplay, &Error::Widget::displayError);
    QObject::connect(m_mover, &Recording::SampleMover::recordingStateChanged, m_configPane, &ConfigPane::recordingStateChanged);
    QObject::connect(m_mover, &Recording::SampleMover::recordingStateChanged, m_trackController, &Recording::TrackController::onRecordingStateChanged);
    QObject::connect(m_mover, &Recording::SampleMover::newRecordingFile, m_trackController, &Recording::TrackController::onRecordingFileChanged);
    QObject::connect(m_mover, &Recording::SampleMover::timeUpdate, m_trackController, &Recording::TrackController::onTimeUpdate);

    QObject::connect(m_trackController, &Recording::TrackController::currentTrackTimeChanged, this, &MainWindow::trackTimeUpdate);

    QObject::connect(m_quitDialog, &QDialog::finished, this, &MainWindow::quitDialogFinished);

    ui->levelL->setMinimum(0);
    ui->levelL->setMaximum(std::numeric_limits<int16_t>::max());
    ui->levelR->setMinimum(0);
    ui->levelR->setMaximum(std::numeric_limits<int16_t>::max());

    ui->tabWidget->addTab(m_configPane, tr("Configuration"));
    ui->tabWidget->addTab(m_trackPane, tr("Recording"));
#ifndef QT_NO_DEBUG
    ui->tabWidget->addTab(new Error::SimulationWidget(this), "Debug");
#endif
    ui->tabWidget->addTab(new AboutPane(this), tr("About"));

    m_configPane->init();
}

MainWindow::~MainWindow()
{
    m_moverThread->quit();
    m_moverThread->wait();
    delete m_moverThread;
}

void
MainWindow::levelMeterUpdate(int16_t left, int16_t right)
{
    ui->levelL->setValue(qBound((int16_t)0, left, std::numeric_limits<int16_t>::max()));
    ui->levelR->setValue(qBound((int16_t)0, right, std::numeric_limits<int16_t>::max()));
}

void
MainWindow::timeUpdate(uint64_t samples)
{
    ui->totalRecTimeLbl->setText(Util::formatTime(samples));
    ui->diskSpaceLbl->setText(Util::formatTime(m_lastSpaceCheckSpace/4 - (samples - m_lastSpaceCheckTime)));
}

void MainWindow::availableSpaceUpdate(uint64_t space)
{
    m_lastSpaceCheckSpace = space;
    m_lastSpaceCheckTime  = m_mover->getRecordedSampleCount();

    ui->diskSpaceLbl->setText(Util::formatTime(space/4));
}

void MainWindow::trackTimeUpdate(uint64_t samples)
{
    ui->trackTimeLbl->setText(Util::formatTime(samples));
}


void MainWindow::trackBtnClicked()
{
    m_trackController->startNewTrack(m_mover->getRecordedSampleCount());
}

void MainWindow::quitDialogFinished(int result)
{
    if (result == QuitDialog::ABORT)
        return;

    if (result == QuitDialog::DISCARD_DATA) {
        // delete all data files
        auto trackFiles = m_config->readAllFiles();
        for (const auto& file : trackFiles) {
            if (!QFile::remove(file.second))
                qWarning() << "Couldn't remove file: " << file.second;
        }
        // clear the database
        m_config->clearTracksAndFiles();
    }

    // quit
    QCoreApplication::quit();
}


void MainWindow::closeEvent(QCloseEvent *ev)
{
    if (m_trackController->getTrackCount()) {
        ev->ignore();

        m_quitDialog->show();
    } else {
        ev->accept();
    }
}
