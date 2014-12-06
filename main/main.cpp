#include "mainwindow.h"
#include <QApplication>

#include <QFont>
#include <QFontDatabase>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // setup font substitution, since windows fonts miss some glyphs
    QFontDatabase::addApplicationFont(":/DejaVuSans.ttf");

    //Doesn't work in WinXP with Qt5.1 :(
    //QFont::insertSubstitution("Tahoma", "DejaVu Sans");

    // create and show the main window
    MainWindow w;

    a.setFont(QFont("DejaVu Sans", w.font().pointSize()));

    w.show();

    return a.exec();
}
