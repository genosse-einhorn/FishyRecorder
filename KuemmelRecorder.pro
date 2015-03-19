#-------------------------------------------------
#
# Project created by QtCreator 2014-05-20T21:31:18
#
#-------------------------------------------------

QT       += core widgets concurrent

CONFIG   += link_pkgconfig c++11
CONFIG   -= exceptions rtti

PKGCONFIG += portaudio-2.0 sqlite3

TARGET   = KuemmelRecorder
TEMPLATE = app

LIBS += -lmp3lame

SOURCES += \
    recording/samplemover.cpp \
    recording/trackcontroller.cpp \
    recording/trackviewpane.cpp \
    recording/trackdataaccessor.cpp \
    export/progressdialog.cpp \
    export/encodedfileexporter.cpp \
    export/wavfileexporter.cpp \
    export/exportbuttongroup.cpp \
    config/database.cpp \
    recording/playbackslave.cpp \
    recording/playbackcontrol.cpp \
    main/configpane.cpp \
    util/diskspace.cpp \
    main/quitdialog.cpp \
    util/customqtmetatypes.cpp \
    error/provider.cpp \
    error/widget.cpp \
    main/main.cpp \
    main/mainwindow.cpp \
    export/mp3fileexporter.cpp \
    main/aboutpane.cpp \
    export/coordinator.cpp \
    main/fancyprogressbar.cpp \
    util/boolsignalor.cpp \
    util/portaudio.cpp \
    presentation/powerpointpresenter.cpp

HEADERS += \
    recording/samplemover.h \
    recording/trackcontroller.h \
    util/misc.h \
    recording/trackviewpane.h \
    recording/trackdataaccessor.h \
    export/progressdialog.h \
    export/encodedfileexporter.h \
    export/wavfileexporter.h \
    export/exportbuttongroup.h \
    config/database.h \
    recording/playbackslave.h \
    recording/playbackcontrol.h \
    main/configpane.h \
    util/diskspace.h \
    main/quitdialog.h \
    util/customqtmetatypes.h \
    error/provider.h \
    error/widget.h \
    main/mainwindow.h \
    export/mp3fileexporter.h \
    main/aboutpane.h \
    export/coordinator.h \
    main/fancyprogressbar.h \
    util/boolsignalor.h \
    util/portaudio.h \
    presentation/powerpointpresenter.h

FORMS    += \
    main/mainwindow.ui \
    export/progressdialog.ui \
    export/exportbuttongroup.ui \
    recording/playbackcontrol.ui \
    main/configpane.ui \
    main/quitdialog.ui \
    main/aboutpane.ui \
    presentation/powerpointpresenter.ui

win32 {
    # presentation subsystem is windows only (unfortunately)

    #FIXME: poppler-qt5.pc is incomplete in MXE, need to work around that
    PKGCONFIG += poppler-qt5 lcms2
    QT += xml

    QT += winextras axcontainer

    SOURCES +=  \
        presentation/presentationtab.cpp \
        presentation/screenviewcontrol.cpp \
        external/qwinhost.cpp \
        presentation/welcomepane.cpp \
        presentation/pdfpresenter.cpp
    HEADERS += \
        presentation/presentationtab.h \
        presentation/screenviewcontrol.h \
        external/qwinhost.h \
        presentation/welcomepane.h \
        presentation/pdfpresenter.h
    FORMS += \
        presentation/presentationtab.ui \
        presentation/welcomepane.ui \
        presentation/pdfpresenter.ui
}

CONFIG(debug, debug|release) {
    CONFIG += console

    SOURCES += \
        error/simulationwidget.cpp \
        error/simulation.cpp

    HEADERS += \
        error/simulation.h \
        error/simulationwidget.h
}


# copy-pasta code deserves to be separated
SOURCES += \
    external/pa_ringbuffer.c

HEADERS += \
    external/pa_ringbuffer.h \
    external/pa_memorybarrier.h

INCLUDEPATH += external

RESOURCES += \
    res/stuff.qrc
