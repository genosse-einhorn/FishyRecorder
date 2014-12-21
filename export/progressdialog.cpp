#include "progressdialog.h"
#include "ui_progressdialog.h"

#include <QDateTime>
#include <QCloseEvent>
#include <QStyle>
#include <QDebug>

namespace Export {

ProgressDialog::ProgressDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ProgressDialog),
    warning_icon(this->style()->standardIcon(QStyle::SP_MessageBoxWarning)),
    info_icon(this->style()->standardIcon(QStyle::SP_MessageBoxInformation)),
    error_icon(this->style()->standardIcon(QStyle::SP_MessageBoxCritical))
{
    ui->setupUi(this);

    // set up the log table in code
    ui->logTable->setColumnCount(3);
    ui->logTable->setRowCount(0);
    ui->logTable->setHorizontalHeaderLabels({ tr("Time"), tr("Message"), tr("Description") });
    ui->logTable->setSelectionMode(QAbstractItemView::NoSelection);
    ui->logTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->logTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->logTable->horizontalHeader()->setStretchLastSection(true);
    ui->logTable->setTextElideMode(Qt::ElideNone);

    ui->cancelBtn->setEnabled(true);
    ui->closeBtn->setEnabled(false);

    ui->progressBar->setRange(0, std::numeric_limits<int>::max());

    QObject::connect(ui->cancelBtn, &QAbstractButton::clicked, this, &ProgressDialog::cancelButtonClicked);
    QObject::connect(ui->closeBtn, &QAbstractButton::clicked, this, &ProgressDialog::closeButtonClicked);
}

ProgressDialog::~ProgressDialog()
{
    delete ui;
}

void ProgressDialog::displayError(Error::Provider::ErrorType type, const QString &message1, const QString &message2)
{
    // first, the icon
    QTableWidgetItem *iconItem = nullptr;
    switch (type) {
    case Error::Provider::ErrorType::Notice:
        iconItem = new QTableWidgetItem(info_icon, "");
        break;
    case Error::Provider::ErrorType::TemporaryWarning:
    case Error::Provider::ErrorType::Warning:
        iconItem = new QTableWidgetItem(warning_icon, "");
        break;
    case Error::Provider::ErrorType::Error:
        iconItem = new QTableWidgetItem(error_icon, "");
        break;
    case Error::Provider::ErrorType::NoError:
        return;
    }

    QTableWidgetItem *timeItem = new QTableWidgetItem(QTime::currentTime().toString("HH:mm:ss"));
    QTableWidgetItem *titleItem = new QTableWidgetItem(message1);
    QTableWidgetItem *textItem = new QTableWidgetItem(message2);

    // resize the table
    auto ourRow = ui->logTable->rowCount();
    ui->logTable->setRowCount(ourRow + 1);

    // insert the items
    ui->logTable->setVerticalHeaderItem(ourRow, iconItem);
    ui->logTable->setItem(ourRow, 0, timeItem);
    ui->logTable->setItem(ourRow, 1, titleItem);
    ui->logTable->setItem(ourRow, 2, textItem);

    ui->logTable->scrollToBottom();
}

void ProgressDialog::setProgress(double progress)
{
    ui->progressBar->setValue((int)(qBound(0.0d, progress, 1.0d) * (double)std::numeric_limits<int>::max()));
}

void ProgressDialog::finished(bool success, const QString &message)
{
    if (success)
        ui->topIcon->setPixmap(info_icon.pixmap(QSize(16, 16)));
    else
        ui->topIcon->setPixmap(error_icon.pixmap(QSize(16, 16)));

    ui->topLbl->setText(message);
    ui->progressBar->setValue(ui->progressBar->maximum());
    ui->cancelBtn->setEnabled(false);
    ui->closeBtn->setEnabled(true);
}

void ProgressDialog::cancelButtonClicked()
{
    emit cancelClicked();
    ui->cancelBtn->setEnabled(false);
}

void ProgressDialog::closeButtonClicked()
{
    done(QDialog::Accepted);
}

void ProgressDialog::closeEvent(QCloseEvent *event)
{
    if (ui->closeBtn->isEnabled())
        event->accept();
    else
        event->ignore();
}

} // namespace Export



