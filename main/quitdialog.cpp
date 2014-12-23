#include "quitdialog.h"
#include "ui_quitdialog.h"

QuitDialog::QuitDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::QuitDialog)
{
    ui->setupUi(this);

    QObject::connect(ui->buttonBox,   &QDialogButtonBox::rejected, this, &QDialog::reject);
    QObject::connect(ui->discardBtn, &QAbstractButton::clicked, this, &QuitDialog::discardClicked);
    QObject::connect(ui->keepBtn,    &QAbstractButton::clicked, this, &QuitDialog::keepClicked);

    ui->topPart->setBackgroundRole(QPalette::Base);
}

QuitDialog::~QuitDialog()
{
    delete ui;
}

void QuitDialog::abortClicked()
{
    done(ABORT);
}

void QuitDialog::keepClicked()
{
    done(KEEP_DATA);
}

void QuitDialog::discardClicked()
{
    done(DISCARD_DATA);
}
