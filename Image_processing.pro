#-------------------------------------------------
#
# Project created by QtCreator 2013-04-28T19:36:04
#
#-------------------------------------------------

QT       += core gui

TARGET = Image_processing
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    mythread.cpp \
    my_main_qlabel.cpp \
    My_label.cpp

HEADERS  += mainwindow.h \
    mythread.h \
    my_main_qlabel.h \
    My_label.h

LIBS += -lopencv_core \
        -lopencv_imgproc \
        -lopencv_highgui \
        -lzmq

FORMS    += \
    mainwindow.ui

RESOURCES += \
    Obrazky.qrc
