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
    ui->latencySlider->setMinimum(0);
    ui->latencySlider->setMaximum(std::numeric_limits<int>::max());
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

    QString lastInputDevice = m_config->readConfigString("input_device");
    QString lastMonitorDevice = m_config->readConfigString("monitor_device");
    double lastMinLatency = m_config->readConfigDouble("last_observed_min_latency");
    double lastMaxLatency = m_config->readConfigDouble("last_observed_max_latency");
    double latency = m_config->readConfigDouble("configured_latency");

    if (lastInputDevice.size())
        ui->recordDevCombo->setCurrentText(lastInputDevice);
    if (lastMonitorDevice.size())
        ui->monitorDevCombo->setCurrentText(lastMonitorDevice);

    double minLatency;
    double maxLatency;

    PaDeviceIndex input_device   = ui->recordDevCombo->currentData().toInt();
    PaDeviceIndex monitor_device = ui->monitorDevCombo->currentData().toInt();

    if (Util::PortAudio::latencyBounds(input_device, monitor_device, &minLatency, &maxLatency)
        && minLatency == lastMinLatency && maxLatency == lastMaxLatency) {

        qDebug() << "Restoring latency: " << latency;
        emit latencyChanged(qBound(minLatency, latency, maxLatency));
    }

    QString lastAudioDir = m_config->readConfigString("audio_data_dir", QStandardPaths::writableLocation(QStandardPaths::TempLocation));
    ui->dataLocationField->setText(lastAudioDir);

    // setup timer which calculates free space
    QTimer *t = new QTimer(this);
    t->setInterval(1000*60*2);
    QObject::connect(t, &QTimer::timeout, this, &ConfigPane::updateAvailableSpace);
    t->start();

    ui->screenCombo->addItem(tr("< No Screen >"), QRect(0, 0, 0, 0));
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

    // find configured screen
    QStringList screenSpec = m_config->readConfigString("presentation_screen").split(',');
    if (screenSpec.size() >= 4) {
        QRect screen(screenSpec[0].toInt(), screenSpec[1].toInt(), screenSpec[2].toInt(), screenSpec[3].toInt());

        for (int i = 0; i < ui->screenCombo->count(); ++i) {
            if (ui->screenCombo->itemData(i).toRect() != screen)
                continue;

            ui->screenCombo->setCurrentIndex(i);
            break;
        }
    }

    QObject::connect(ui->screenCombo, SELECT_SIGNAL_OVERLOAD<int>::OF(&QComboBox::currentIndexChanged), this, &ConfigPane::screenComboChanged);
    QObject::connect((QGuiApplication*)QGuiApplication::instance(), &QGuiApplication::screenAdded, this, &ConfigPane::screenAdded);
    QObject::connect((QGuiApplication*)QGuiApplication::instance(), &QGuiApplication::screenRemoved, this, &ConfigPane::screenRemoved);

    screenComboChanged(ui->screenCombo->currentIndex());
}

void ConfigPane::displayDeviceError(Error::Provider::ErrorType type, const QString &str1, const QString &str2)
{
    ui->deviceErrorWidget->displayError(type, str1, str2);
}

void ConfigPane::recordingStateChanged(bool recording)
{
    ui->audioDeviceGrp->setEnabled(!recording);
}

void ConfigPane::handleLatencyChanged(double min, double max, double value)
{
    int slidermin = ui->latencySlider->minimum();
    int slidermax = ui->latencySlider->maximum();

    m_maxLatency = max;
    m_minLatency = min;

    m_config->writeConfigDouble("last_observed_min_latency", min);
    m_config->writeConfigDouble("last_observed_max_latency", max);
    m_config->writeConfigDouble("configured_latency", value);

    qDebug() << "Saving latency=" << value << " last min=" << min << " max=" << max;

    int sliderval = slidermin + static_cast<int>((value - min)/(max - min)*(double)(slidermax - slidermin));

    {
        Util::SignalBlocker blocker(ui->latencySlider);
        ui->latencySlider->setValue(sliderval);
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

void ConfigPane::screenComboChanged(int index)
{
    QRect screen = ui->screenCombo->itemData(index).toRect();

    m_config->writeConfigString("presentation_screen",
                                QString("%1,%2,%3,%4").arg(screen.x()).arg(screen.y()).arg(screen.width()).arg(screen.height()));

    emit presentationScreenChanged(screen);
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

void ConfigPane::screenAdded(QScreen *screen)
{
    ui->screenCombo->addItem(
                QString("%1 [%2x%3 at %4,%5]")
                    .arg(screen->name())
                    .arg(screen->size().width())
                    .arg(screen->size().height())
                    .arg(screen->geometry().left())
                    .arg(screen->geometry().top()),
                screen->geometry());
}

void ConfigPane::screenRemoved(QScreen *screen)
{
    for (int i = 0; i < ui->screenCombo->count(); ++i) {
        if (ui->screenCombo->itemData(i).toRect() == screen->geometry())
            ui->screenCombo->removeItem(i);
    }
}

void ConfigPane::latencySliderMoved(int value)
{
    int slidermin = ui->latencySlider->minimum();
    int slidermax = ui->latencySlider->maximum();
    double latency = m_minLatency + static_cast<double>(value - slidermin)/static_cast<double>(slidermax - slidermin)*(m_maxLatency - m_minLatency);

    emit latencyChanged(latency);
}
