#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <memory>

namespace Ui {
    class MainWindow;
}

namespace Presentation {
    class PresentationTab;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private:
    std::unique_ptr<Ui::MainWindow> ui;
    Presentation::PresentationTab *m_presentation = nullptr;
};

#endif // MAINWINDOW_H
