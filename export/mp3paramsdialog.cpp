#include "mp3paramsdialog.h"
#include "ui_mp3paramsdiaog.h"

namespace Export {

Mp3ParamsDialog::Mp3ParamsDialog(const QString &defaultAlbum, const QString &defaultArtist, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Mp3ParamsDiaog)
{
    ui->setupUi(this);

    for (int brate : { 128, 160, 192, 224, 256, 320 }) {
        ui->brateCombo->addItem(tr("%1 kbit/s").arg(brate), QVariant::fromValue(brate));
    }
    ui->brateCombo->setCurrentIndex(2); // 192 kbit/s

    ui->albumField->setText(defaultAlbum);
    ui->artistField->setText(defaultArtist);
}

Mp3ParamsDialog::~Mp3ParamsDialog()
{
    delete ui;
}

QString Mp3ParamsDialog::album() const
{
    return ui->albumField->text();
}

QString Mp3ParamsDialog::artist() const
{
    return ui->artistField->text();
}

int Mp3ParamsDialog::brate() const
{
    return ui->brateCombo->currentData().toInt();
}

} // namespace Export
