#include "exportbuttongroup.h"
#include "ui_exportbuttongroup.h"

#include "recording/trackcontroller.h"
#include "export/wavfileexporter.h"
#include "export/mp3fileexporter.h"
#include "export/progressdialog.h"

#include <QFileDialog>
#include <QThread>
#include <QTimer>

namespace {
    void kickoffExport(Export::EncodedFileExporter *exporter, Export::ProgressDialog *dialog)
    {
        QThread *worker = new QThread();

        exporter->moveToThread(worker);

        // wire up exporter and progress dialog
        QObject::connect(exporter, &Export::EncodedFileExporter::percentage, dialog, &Export::ProgressDialog::setProgress);
        QObject::connect(exporter, &Export::EncodedFileExporter::finished, dialog, &Export::ProgressDialog::finished);
        QObject::connect(exporter->errorProvider(), &Error::Provider::error, dialog, &Export::ProgressDialog::displayError);
        QObject::connect(dialog, &Export::ProgressDialog::cancelClicked, exporter, &Export::EncodedFileExporter::abort);

        // install automatic cleanup routines
        QObject::connect(dialog, &QDialog::finished, dialog, &QObject::deleteLater);
        QObject::connect(dialog, &QObject::destroyed, exporter, &QObject::deleteLater);
        QObject::connect(dialog, &QDialog::destroyed, worker, &QThread::quit);
        QObject::connect(worker, &QThread::finished, worker, &QThread::deleteLater);

        // show the dialog and arrange for the worker to start up
        QObject::connect(worker, &QThread::started, exporter, &Export::EncodedFileExporter::start);
        dialog->open();
        worker->start();
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

    kickoffExport(new WavFileExporter(m_controller, dir), dialog);
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

    kickoffExport(new Mp3FileExporter(m_controller, dir), dialog);
}

} // namespace Export
