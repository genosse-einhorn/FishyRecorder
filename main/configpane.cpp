#include "configpane.h"
#include "ui_configpane.h"
#include "util/misc.h"
#include "util/diskspace.h"
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

    QString lastInputDevice = m_config->readConfigString("input_device");
    QString lastMonitorDevice = m_config->readConfigString("monitor_device");
    if (lastInputDevice.size())
        ui->recordDevCombo->setCurrentText(lastInputDevice);
    if (lastMonitorDevice.size())
        ui->monitorDevCombo->setCurrentText(lastMonitorDevice);

    QString lastAudioDir = m_config->readConfigString("audio_data_dir", QStandardPaths::writableLocation(QStandardPaths::TempLocation));
    ui->dataLocationField->setText(lastAudioDir);

    // setup timer which calculates free space
    QTimer *t = new QTimer(this);
    t->setInterval(1000*60*2);
    QObject::connect(t, &QTimer::timeout, this, &ConfigPane::updateAvailableSpace);
    t->start();

#ifdef Q_OS_WIN32
    QObject::connect(ui->screenCombo, SELECT_SIGNAL_OVERLOAD<int>::OF(&QComboBox::currentIndexChanged), this, &ConfigPane::screenComboChanged);

    for (QScreen* screen : QGuiApplication::screens()) {
        ui->screenCombo->addItem(
                    QString("%1 [%2x%3 at %4,%5]")
                        .arg(screen->name())
                        .arg(screen->size().width())
                        .arg(screen->size().height())
                        .arg(screen->geometry().left())
                        .arg(screen->geometry().top()),
                    screen->geometry());
    }
#else
    ui->screenCombo->setEnabled(false);
#endif
}

void ConfigPane::displayDeviceError(Error::Provider::ErrorType type, const QString &str1, const QString &str2)
{
    ui->deviceErrorWidget->displayError(type, str1, str2);
}

void ConfigPane::recordingStateChanged(bool recording)
{
    ui->audioDeviceGrp->setEnabled(!recording);
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

void ConfigPane::screenComboChanged(int index)
{
    //TODO: write into config?
    emit presentationScreenChanged(ui->screenCombo->itemData(index).toRect());
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
