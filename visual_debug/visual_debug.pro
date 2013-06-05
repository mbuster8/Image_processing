#-------------------------------------------------
#
# Project created by QtCreator 2013-04-28T20:11:15
#
#-------------------------------------------------

QT       -= gui

TARGET = visual_debug
TEMPLATE = lib

DEFINES += VISUAL_DEBUG_LIBRARY

SOURCES += visual_debug.cpp

HEADERS += visual_debug.h

LIBS += -lopencv_core \
        -lzmq

symbian {
    MMP_RULES += EXPORTUNFROZEN
    TARGET.UID3 = 0xE428B7AA
    TARGET.CAPABILITY = 
    TARGET.EPOCALLOWDLLDATA = 1
    addFiles.sources = visual_debug.dll
    addFiles.path = !:/sys/bin
    DEPLOYMENT += addFiles
}

unix:!symbian {
    maemo5 {
        target.path = /opt/usr/lib
    } else {
        target.path = /usr/lib
    }
    INSTALLS += target
}
