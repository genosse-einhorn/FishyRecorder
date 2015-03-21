#ifndef EXPORT_MP3PARAMSDIAOG_H
#define EXPORT_MP3PARAMSDIAOG_H

#include <QDialog>

namespace Export {

namespace Ui {
class Mp3ParamsDialog;
}

class Mp3ParamsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit Mp3ParamsDialog(const QString &defaultAlbum = QString(), const QString &defaultArtist = QString(), QWidget *parent = 0);
    ~Mp3ParamsDialog();

    QString album() const;
    QString artist() const;
    int     brate() const;

private:
    Ui::Mp3ParamsDialog *ui;
};


} // namespace Export
#endif // EXPORT_MP3PARAMSDIAOG_H
