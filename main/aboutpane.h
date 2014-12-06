#ifndef ABOUTPANE_H
#define ABOUTPANE_H

#include <QWidget>

namespace Ui {
class AboutPane;
}

class AboutPane : public QWidget
{
    Q_OBJECT

public:
    explicit AboutPane(QWidget *parent = 0);
    ~AboutPane();

private:
    Ui::AboutPane *ui;
};

#endif // ABOUTPANE_H
