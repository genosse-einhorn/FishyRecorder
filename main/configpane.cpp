#include "configpane.h"
#include "ui_configpane.h"
#include "util/misc.h"
#include "util/diskspace.h"
#include "util/portaudio.h"
#include "config/database.h"

#include <QStandardPaths>
#include <QFileDialog>
#include <QTimer>
#include <QScreen>

#ifdef Q_OS_WIN32
#   include <windows.h>
#endif

ConfigPane::ConfigPane(Config::Database *db, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ConfigPane),
    m_config(db)
{
    ui->setupUi(this);

    // setup the latency field
    ui->latencySlider->setMinimum(5000); // in us
    ui->latencySlider->setMaximum(500000);
    ui->latencySlider->setValue(ui->latencySlider->maximum());
    ui->volumeSlider->setValue(1000000000);
    ui->volumeSlider->setTickPosition(QSlider::TicksBelow);
}

ConfigPane::~ConfigPane()
{
    delete ui;
}

void ConfigPane::init()
{
    ui->recordDevCombo->addItem(tr("[ no device ]"), paNoDevice);
    ui->monitorDevCombo->addItem(tr("[ no device ]"), paNoDevice);

    // fill with audio devices
    int maxdev = Pa_GetDeviceCount();
    for (int i = 0; i < maxdev; ++i) {
        const PaDeviceInfo *info = Pa_GetDeviceInfo(i);

        if (info->maxInputChannels > 0)
            ui->recordDevCombo->addItem(QString::fromUtf8(info->name), i);

        if (info->maxOutputChannels > 0)
            ui->monitorDevCombo->addItem(QString::fromUtf8(info->name), i);
    }

    QObject::connect(ui->recordDevCombo, SELECT_SIGNAL_OVERLOAD<int>::OF(&QComboBox::currentIndexChanged), this, &ConfigPane::recordDevComboChanged);
    QObject::connect(ui->monitorDevCombo, SELECT_SIGNAL_OVERLOAD<int>::OF(&QComboBox::currentIndexChanged), this, &ConfigPane::monitorDevComboChanged);
    QObject::connect(ui->dataLocationBtn, &QAbstractButton::clicked, this, &ConfigPane::audioDataBtnClicked);
    QObject::connect(ui->dataLocationField, &QLineEdit::textChanged, this, &ConfigPane::audioDataFieldChanged);
    QObject::connect(ui->latencySlider, &QAbstractSlider::valueChanged, this, &ConfigPane::latencySliderMoved);
    QObject::connect(ui->volumeSlider, &QAbstractSlider::valueChanged, this, &ConfigPane::volumeSliderMoved);

    QString lastInputDevice = m_config->readConfigString("input_device");
    QString lastMonitorDevice = m_config->readConfigString("monitor_device");
    double latency = m_config->readConfigDouble("configured_latency", 1.0);
    double multiplicator = m_config->readConfigDouble("volume_multiplicator", 1.0);

    if (lastInputDevice.size())
        ui->recordDevCombo->setCurrentText(lastInputDevice);
    if (lastMonitorDevice.size())
        ui->monitorDevCombo->setCurrentText(lastMonitorDevice);

    qDebug() << "Restoring latency=" << latency << " volume=" << multiplicator;
    emit latencyChanged(qBound(ui->latencySlider->minimum() * 0.000001, latency, ui->latencySlider->maximum() * 0.000001));
    emit volumeMultiplicatorChanged(float(multiplicator));

    QString lastAudioDir = m_config->readConfigString("audio_data_dir", QStandardPaths::writableLocation(QStandardPaths::TempLocation));
    ui->dataLocationField->setText(lastAudioDir);

    // setup timer which calculates free space
    QTimer *t = new QTimer(this);
    t->setInterval(1000*60*2);
    QObject::connect(t, &QTimer::timeout, this, &ConfigPane::updateAvailableSpace);
    t->start();
}

void ConfigPane::displayDeviceError(Error::Provider::ErrorType type, const QString &str1, const QString &str2)
{
    ui->deviceErrorWidget->displayError(type, str1, str2);
}

void ConfigPane::recordingStateChanged(bool recording)
{
    ui->audioDeviceGrp->setEnabled(!recording);
}

void ConfigPane::handleLatencyChanged(double valueInS)
{
    int valueInUs = valueInS * 1000000;

    m_config->writeConfigDouble("configured_latency", valueInS);

    qDebug() << "Saving latency=" << valueInS;

    {
        Util::SignalBlocker blocker(ui->latencySlider);
        ui->latencySlider->setValue(valueInUs);
    }
}

void ConfigPane::handleVolumeMultiplicatorChanged(float multiplicator)
{
    float sliderVal = std::pow(multiplicator, 1.0f/3.0f);

    m_config->writeConfigDouble("volume_multiplicator", multiplicator);

    qDebug() << "Saving volume " << multiplicator << "";

    {
        Util::SignalBlocker blocker(ui->volumeSlider);
        ui->volumeSlider->setValue(sliderVal * 1000000000);
    }
}

void ConfigPane::recordDevComboChanged(int index)
{
    m_config->writeConfigString("input_device", ui->recordDevCombo->currentText());
    emit recordDeviceChanged(ui->recordDevCombo->itemData(index).toInt());
}

void ConfigPane::monitorDevComboChanged(int index)
{
    m_config->writeConfigString("monitor_device", ui->monitorDevCombo->currentText());
    emit monitorDeviceChanged(ui->monitorDevCombo->itemData(index).toInt());
}

void ConfigPane::audioDataBtnClicked()
{
    QString newDir = QFileDialog::getExistingDirectory(this,
                                                       tr("Select the directory to hold temporary audio data"),
                                                       ui->dataLocationField->text(),
                                                       QFileDialog::ShowDirsOnly);

    if (newDir.size())
        ui->dataLocationField->setText(newDir);
}

void ConfigPane::audioDataFieldChanged()
{
    auto newDir = ui->dataLocationField->text();

    m_config->writeConfigString("audio_data_dir", newDir);

    updateAvailableSpace();

    emit audioDataDirChanged(newDir);
}

void ConfigPane::updateAvailableSpace()
{
    uint64_t availableSpace = Util::DiskSpace::availableBytesForPath(ui->dataLocationField->text());

    ui->diskSpaceLbl->setText(Util::DiskSpace::humanReadable(availableSpace, true));

    emit availableAudioSpaceChanged(availableSpace);
}

void ConfigPane::latencySliderMoved(int value)
{
    double latency = value * 0.000001;

    emit latencyChanged(latency);
}

void ConfigPane::volumeSliderMoved(int value)
{
    float multiplicator = std::pow(value/1000000000.0f, 3);

    volumeMultiplicatorChanged(multiplicator);
}
