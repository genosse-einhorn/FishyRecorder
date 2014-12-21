#include "exportbuttongroup.h"
#include "ui_exportbuttongroup.h"

#include "recording/trackcontroller.h"
#include "export/wavfileexporter.h"
#include "export/mp3fileexporter.h"
#include "export/progressdialog.h"
#include "export/coordinator.h"

#include <QFileDialog>
#include <QThread>
#include <QTimer>

namespace {
    void kickoffExport(Export::Coordinator *exporter, Export::ProgressDialog *dialog)
    {
        QObject::connect(exporter, &Export::Coordinator::percentage, dialog, &Export::ProgressDialog::setProgress);
        QObject::connect(exporter, &Export::Coordinator::finished, dialog, &Export::ProgressDialog::finished);
        QObject::connect(exporter, &Export::Coordinator::message, dialog, &Export::ProgressDialog::displayError);
        QObject::connect(dialog, &Export::ProgressDialog::cancelClicked, exporter, &Export::Coordinator::abort);

        // install automatic cleanup routines
        QObject::connect(dialog, &QDialog::finished, dialog, &QObject::deleteLater);
        QObject::connect(dialog, &QObject::destroyed, exporter, &QObject::deleteLater);

        // show the dialog and arrange for the worker to start up
        dialog->open();
        exporter->start();
    }
}

namespace Export {

ExportButtonGroup::ExportButtonGroup(const Recording::TrackController *controller, QWidget *parent) :
    QGroupBox(parent),
    ui(new Ui::ExportButtonGroup),
    m_controller(controller)
{
    ui->setupUi(this);

    QObject::connect(ui->exportAsWavBtn, &QAbstractButton::clicked, this, &ExportButtonGroup::wavBtnClicked);
    QObject::connect(ui->exportAsMp3Btn, &QAbstractButton::clicked, this, &ExportButtonGroup::mp3BtnClicked);
}

ExportButtonGroup::~ExportButtonGroup()
{
    delete ui;
}

void ExportButtonGroup::wavBtnClicked()
{
    QString dir = QFileDialog::getExistingDirectory(this,
                                                    tr("Select the directory to place the WAV files in"),
                                                    QString(),
                                                    QFileDialog::ShowDirsOnly);

    if (!dir.size())
        return;

    auto dialog = new ProgressDialog(this);
    dialog->setWindowTitle(tr("Exporting WAV files"));

    kickoffExport(new Coordinator(m_controller, dir, &WavFileExporter::create, this), dialog);
}

void ExportButtonGroup::mp3BtnClicked()
{
    QString dir = QFileDialog::getExistingDirectory(this,
                                                    tr("Select the directory to place the MP3 files in"),
                                                    QString(),
                                                    QFileDialog::ShowDirsOnly);

    if (!dir.size())
        return;

    auto dialog = new ProgressDialog(this);
    dialog->setWindowTitle(tr("Exporting MP3 files"));

    kickoffExport(new Coordinator(m_controller, dir, &Mp3FileExporter::create, this), dialog);
}

} // namespace Export
