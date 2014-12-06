#ifndef EXPORT_EXPORTBUTTONGROUP_H
#define EXPORT_EXPORTBUTTONGROUP_H

#include <QGroupBox>

namespace Recording {
    class TrackController;
}

namespace Export {

namespace Ui {
class ExportButtonGroup;
}

class ExportButtonGroup : public QGroupBox
{
    Q_OBJECT

public:
    explicit ExportButtonGroup(const Recording::TrackController *controller, QWidget *parent = 0);
    ~ExportButtonGroup();

private slots:
    void wavBtnClicked();
    void mp3BtnClicked();

private:
    Ui::ExportButtonGroup *ui;
    const Recording::TrackController *m_controller;
};


} // namespace Export
#endif // EXPORT_EXPORTBUTTONGROUP_H
