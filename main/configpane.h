#ifndef CONFIGPANE_H
#define CONFIGPANE_H

#include <QWidget>
#include <portaudio.h>
#include <error/provider.h>

namespace Ui {
class ConfigPane;
}

namespace Config {
class Database;
}

class ConfigPane : public QWidget
{
    Q_OBJECT

public:
    explicit ConfigPane(Config::Database *db, QWidget *parent = 0);
    ~ConfigPane();

    //!
    //! \brief Reads existing values from the database and fires change events
    //!
    //! Call this after you have successfully wired up the signals.
    //!
    void init();

signals:
    void recordDeviceChanged(PaDeviceIndex index);
    void monitorDeviceChanged(PaDeviceIndex index);
    void audioDataDirChanged(const QString& dir);
    void availableAudioSpaceChanged(uint64_t freeBytes);
    void latencyChanged(double value);
    void volumeMultiplicatorChanged(float multiplicator);

public slots:
    void displayDeviceError(Error::Provider::ErrorType type, const QString& str1, const QString& str2);
    void recordingStateChanged(bool recording);
    void handleLatencyChanged(double valueInS);
    void handleVolumeMultiplicatorChanged(float multiplicator);

private slots:
    void recordDevComboChanged(int index);
    void monitorDevComboChanged(int index);
    void audioDataBtnClicked();
    void audioDataFieldChanged();
    void updateAvailableSpace();
    void latencySliderMoved(int value);
    void volumeSliderMoved(int value);

private:
    Ui::ConfigPane   *ui;
    Config::Database *m_config;
};

#endif // CONFIGPANE_H
