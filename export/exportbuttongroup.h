#ifndef EXPORT_EXPORTBUTTONGROUP_H
#define EXPORT_EXPORTBUTTONGROUP_H

#include <QGroupBox>

namespace Recording {
    class TrackController;
}

namespace Config {
    class Database;
}

namespace Export {

namespace Ui {
class ExportButtonGroup;
}

class ExportButtonGroup : public QGroupBox
{
    Q_OBJECT

public:
    explicit ExportButtonGroup(const Recording::TrackController *controller, Config::Database *db, QWidget *parent = 0);
    ~ExportButtonGroup();

private slots:
    void wavBtnClicked();
    void mp3BtnClicked();
    void flacBtnClicked();

private:
    Ui::ExportButtonGroup *ui;
    const Recording::TrackController *m_controller;
    Config::Database                 *m_database;
};


} // namespace Export
#endif // EXPORT_EXPORTBUTTONGROUP_H
