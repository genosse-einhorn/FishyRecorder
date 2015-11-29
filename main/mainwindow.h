#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <memory>

namespace Ui {
    class MainWindow;
}

namespace Recording {
    class SampleMover;
    class TrackController;
    class TrackViewPane;
}

namespace Config {
    class Database;
}

namespace Presentation {
    class PresentationTab;
}

class ConfigPane;
class QuitDialog;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void levelMeterUpdate(float left, float right);
    void timeUpdate(uint64_t samples);
    void availableSpaceUpdate(uint64_t space);
    void trackTimeUpdate(uint64_t samples);
    void trackBtnClicked();
    void quitDialogFinished(int result);

private:
    std::unique_ptr<Ui::MainWindow> ui;
    Config::Database           *m_config;
    Recording::SampleMover     *m_mover;
    Recording::TrackController *m_trackController;
    Recording::TrackViewPane   *m_trackPane;
    ConfigPane                 *m_configPane;
    QuitDialog                 *m_quitDialog;
    QThread                    *m_moverThread;

    uint64_t m_lastSpaceCheckTime  = 0;
    uint64_t m_lastSpaceCheckSpace = 0;

    Presentation::PresentationTab *m_presentation = nullptr;

    // QWidget interface
protected:
    virtual void closeEvent(QCloseEvent *);
};

#endif // MAINWINDOW_H
