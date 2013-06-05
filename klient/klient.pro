#-------------------------------------------------
#
# Project created by QtCreator 2013-04-28T20:06:23
#
#-------------------------------------------------

QT       += core gui

TARGET = klient
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp

HEADERS  += mainwindow.h \
    visual_debug.h

FORMS    += \
    mainwindow.ui

LIBS += -lopencv_core \
        -lopencv_imgproc \
        -lopencv_highgui \
        -lzmq \

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../klient-build-desktop-Qt_4_8_1_in_PATH__System__Debug/release/ -lvisual_debug
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../klient-build-desktop-Qt_4_8_1_in_PATH__System__Debug/debug/ -lvisual_debug
else:symbian: LIBS += -lvisual_debug
else:unix: LIBS += -L$$PWD/../klient-build-desktop-Qt_4_8_1_in_PATH__System__Debug/ -lvisual_debug

INCLUDEPATH += $$PWD/../klient-build-desktop-Qt_4_8_1_in_PATH__System__Debug
DEPENDPATH += $$PWD/../klient-build-desktop-Qt_4_8_1_in_PATH__System__Debug

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../klient-build-desktop-Qt_4_8_1_in_PATH__System__Release/release/ -lvisual_debug
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../klient-build-desktop-Qt_4_8_1_in_PATH__System__Release/debug/ -lvisual_debug
else:symbian: LIBS += -lvisual_debug
else:unix: LIBS += -L$$PWD/../klient-build-desktop-Qt_4_8_1_in_PATH__System__Release/ -lvisual_debug

INCLUDEPATH += $$PWD/../klient-build-desktop-Qt_4_8_1_in_PATH__System__Release
DEPENDPATH += $$PWD/../klient-build-desktop-Qt_4_8_1_in_PATH__System__Release
