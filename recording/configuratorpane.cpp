#include "configuratorpane.h"
#include "ui_recordingconfiguratorpane.h"

#include <QSettings>
#include <QStandardPaths>
#include <QFileDialog>
#include <portaudio.h>

#include "coordinator.h"

namespace Recording {

ConfiguratorPane::ConfiguratorPane(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::RecordingConfiguratorPane)
{
    Pa_Initialize();

    ui->setupUi(this);

    QSettings settings;

    ui->cbRecordDev->addItem(tr("<No Device>"), QVariant::fromValue(paNoDevice));
    ui->cbMonitorDev->addItem(tr("<No Device>"), QVariant::fromValue(paNoDevice));
    for (PaDeviceIndex i = 0; i < Pa_GetDeviceCount(); ++i)
    {
        const PaDeviceInfo *info = Pa_GetDeviceInfo(i);

        if (Coordinator::isSupportedInput(i, info))
        {
            ui->cbRecordDev->addItem(QString::fromLocal8Bit(info->name), QVariant::fromValue(i));
        }

        if (Coordinator::isSupportedOutput(i, info))
        {
            ui->cbMonitorDev->addItem(QString::fromLocal8Bit(info->name), QVariant::fromValue(i));
        }
    }

    auto i = ui->cbRecordDev->findText(settings.value("Recording Device", QVariant::fromValue(QString())).toString());
    if (i >= 0)
    {
        ui->cbRecordDev->setCurrentIndex(i);
    }
    else
    {
        ui->cbRecordDev->setCurrentIndex(ui->cbRecordDev->findData(QVariant::fromValue(Pa_GetDefaultInputDevice())));
    }

    i = ui->cbMonitorDev->findText(settings.value("Monitor Device", QVariant::fromValue(QString())).toString());
    if (i >= 0)
    {
        ui->cbMonitorDev->setCurrentIndex(i);
    }
    else
    {
        ui->cbMonitorDev->setCurrentIndex(ui->cbMonitorDev->findData(QVariant::fromValue(Pa_GetDefaultOutputDevice())));
    }

    ui->slVolume->setValue(settings.value("Volume", QVariant::fromValue(100000000)).toInt());

    ui->eDirectory->setText(settings.value("Output Directory",
        QVariant::fromValue(QStandardPaths::writableLocation(QStandardPaths::MusicLocation))).toString());

    QString artist = settings.value("MP3 Artist").toString();
    if (!artist.size())
        artist = QString::fromUtf8(qgetenv("USER"));
    if (!artist.size())
        artist = QString::fromUtf8(qgetenv("USERNAME"));
    if (!artist.size())
        artist = tr("Someone");
    ui->eMp3Artist->setText(artist);



    QObject::connect(ui->cbMonitorDev, &QComboBox::currentTextChanged, this, &ConfiguratorPane::cbMonitorDevChanged);
    QObject::connect(ui->cbRecordDev, &QComboBox::currentTextChanged, this, &ConfiguratorPane::cbRecordDevChanged);
    QObject::connect(ui->slVolume, &QSlider::valueChanged, this, &ConfiguratorPane::slVolumeChanged);
    QObject::connect(ui->bPicker, &QAbstractButton::clicked, this, &ConfiguratorPane::outputDirButtonClick);
    QObject::connect(ui->eMp3Artist, &QLineEdit::textChanged, this, &ConfiguratorPane::eMp3ArtistTextChanged);
}

ConfiguratorPane::~ConfiguratorPane()
{
    delete ui;

    Pa_Terminate();
}

void ConfiguratorPane::hookupCoordinator(Coordinator *c)
{
    QObject::connect(this, &ConfiguratorPane::monitorDevChanged, c, &Coordinator::setMonitorDevice);
    QObject::connect(this, &ConfiguratorPane::recordingDevChanged, c, &Coordinator::setRecordingDevice);
    QObject::connect(this, &ConfiguratorPane::volumeChanged, c, &Coordinator::setVolumeFactor);
    QObject::connect(this, &ConfiguratorPane::outputDirChanged, c, &Coordinator::setSaveDir);
    QObject::connect(this, &ConfiguratorPane::mp3ArtistChanged, c, &Coordinator::setMp3ArtistName);

    // initial sync
    cbRecordDevChanged();
    cbMonitorDevChanged();
    slVolumeChanged();
    eMp3ArtistTextChanged();
    emit outputDirChanged(ui->eDirectory->text());
}

void ConfiguratorPane::cbRecordDevChanged()
{
    QSettings().setValue("Recording Device", QVariant::fromValue(ui->cbRecordDev->currentText()));
    emit recordingDevChanged(ui->cbRecordDev->currentData().value<PaDeviceIndex>());
}

void ConfiguratorPane::cbMonitorDevChanged()
{
    QSettings().setValue("Monitor Device", QVariant::fromValue(ui->cbMonitorDev->currentText()));
    emit monitorDevChanged(ui->cbMonitorDev->currentData().value<PaDeviceIndex>());
}

void ConfiguratorPane::slVolumeChanged()
{
    QSettings().setValue("Volume", QVariant::fromValue(ui->slVolume->value()));
    double val = double(ui->slVolume->value()) / 100000000.0;
    emit volumeChanged(val);
}

void ConfiguratorPane::outputDirButtonClick()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Directory"), ui->eDirectory->text());
    if (dir.length())
    {
        ui->eDirectory->setText(dir);

        QSettings().setValue("Output Directory", QVariant::fromValue(dir));

        emit outputDirChanged(dir);
    }
}

void ConfiguratorPane::eMp3ArtistTextChanged()
{
    QSettings().setValue("MP3 Artist", QVariant::fromValue(ui->eMp3Artist->text()));
    emit mp3ArtistChanged(ui->eMp3Artist->text());
}

} // namespace Recording
