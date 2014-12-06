#ifndef EXPORT_PROGRESSDIALOG_H
#define EXPORT_PROGRESSDIALOG_H

#include <QDialog>
#include <QIcon>

#include <error/provider.h>

namespace Export {

namespace Ui {
class ProgressDialog;
}

class ProgressDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ProgressDialog(QWidget *parent = 0);
    ~ProgressDialog();

public slots:
    void displayError(Error::Provider::ErrorType type, const QString& message1, const QString& message2);
    void setProgress(double progress);
    void finished(); //! will activate the close button

private slots:
    void cancelButtonClicked();
    void closeButtonClicked();

signals:
    void cancelClicked();

private:
    Ui::ProgressDialog *ui;

    QIcon warning_icon;
    QIcon info_icon;
    QIcon error_icon;

    // QWidget interface
protected:
    virtual void closeEvent(QCloseEvent *);
};


} // namespace Export
#endif // EXPORT_PROGRESSDIALOG_H
