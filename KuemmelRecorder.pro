#-------------------------------------------------
#
# Project created by QtCreator 2014-05-20T21:31:18
#
#-------------------------------------------------

QT       += core gui widgets

CONFIG   += link_pkgconfig c++11

PKGCONFIG += portaudio-2.0

TARGET   = KuemmelRecorder
TEMPLATE = app

LIBS += -lsqlite3 -lmp3lame

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
    main/aboutpane.cpp
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
    main/aboutpane.h

FORMS    += \
    main/mainwindow.ui \
    export/progressdialog.ui \
    export/exportbuttongroup.ui \
    recording/playbackcontrol.ui \
    main/configpane.ui \
    main/quitdialog.ui \
    main/aboutpane.ui

CONFIG(debug, debug|release) {
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
