#-------------------------------------------------
#
# Project created by QtCreator 2014-05-20T21:31:18
#
#-------------------------------------------------

QT       += core widgets concurrent multimedia multimediawidgets
CONFIG   += link_pkgconfig c++11
CONFIG   -= exceptions rtti

PKGCONFIG += portaudio-2.0 sqlite3 poppler-qt5 flac

TARGET   = KuemmelRecorder
TEMPLATE = app

LIBS += -lmp3lame

SOURCES += \
    main/main.cpp \
    main/mainwindow.cpp \
    main/aboutpane.cpp \
    presentation/presentationtab.cpp \
    presentation/welcomepane.cpp \
    presentation/pdfpresenter.cpp \
    presentation/pdfpreviewwidget.cpp \
    presentation/presentationwindow.cpp \
    presentation/presenterbase.cpp \
    presentation/mediapresenter.cpp

HEADERS += \
    util/misc.h \
    main/mainwindow.h \
    main/aboutpane.h \
    presentation/presentationtab.h \
    presentation/welcomepane.h \
    presentation/pdfpresenter.h \
    presentation/pdfpreviewwidget.h \
    presentation/presentationwindow.h \
    presentation/presenterbase.h \
    presentation/mediapresenter.h

FORMS    += \
    main/mainwindow.ui \
    main/aboutpane.ui \
    presentation/presentationtab.ui \
    presentation/welcomepane.ui \
    presentation/pdfpresenter.ui \
    presentation/mediapresenter.ui

TRANSLATIONS = l10n/recorder_de.ts

win32 {
    RC_ICONS = res/fishy.ico

    #FIXME: poppler-qt5.pc is incomplete in MXE, need to work around that
    PKGCONFIG += poppler-qt5 lcms2 freetype2
    QT += xml

    QT += winextras axcontainer svg
}

CONFIG(debug, debug|release) {
    CONFIG += console
}


RESOURCES += \
    res/stuff.qrc
