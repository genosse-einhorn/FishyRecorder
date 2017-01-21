#include "mainwindow.h"
#include <QApplication>

#include <QFont>
#include <QFontDatabase>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // For QSettings, mainly
    a.setOrganizationDomain("Genosse Einhorn");
    a.setApplicationName("KuemmelRecorder");

    // setup translations
    QTranslator translator;
    translator.load(QLocale::system(), ":/locale/");

    a.installTranslator(&translator);

    // setup font substitution, since windows fonts miss some glyphs
    QFontDatabase::addApplicationFont(":/DejaVuSans.ttf");

    // create and show the main window
    MainWindow w;

    w.show();

    return a.exec();
}
