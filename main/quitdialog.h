#ifndef QUITDIALOG_H
#define QUITDIALOG_H

#include <QDialog>

namespace Ui {
class QuitDialog;
}

class QuitDialog : public QDialog
{
    Q_OBJECT

public:
    explicit QuitDialog(QWidget *parent = 0);
    ~QuitDialog();

    enum Choice {
        ABORT = QDialog::Rejected,
        KEEP_DATA = QDialog::Accepted,
        DISCARD_DATA
    };

private slots:
    void abortClicked();
    void keepClicked();
    void discardClicked();

private:
    Ui::QuitDialog *ui;
};

#endif // QUITDIALOG_H
