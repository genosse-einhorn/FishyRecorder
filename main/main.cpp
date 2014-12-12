#include "mainwindow.h"
#include <QApplication>

#include <QFont>
#include <QFontDatabase>



int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // setup font substitution, since windows fonts miss some glyphs
    QFontDatabase::addApplicationFont(":/DejaVuSans.ttf");

    // create and show the main window
    MainWindow w;

    w.show();

    return a.exec();
}
