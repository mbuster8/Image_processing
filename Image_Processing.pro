#-------------------------------------------------
#
# Project created by QtCreator 2012-11-27T19:27:14
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = Image_Processing
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    My_label.cpp \
    mythread.cpp \
    my_main_qlabel.cpp

HEADERS  += mainwindow.h \
    My_label.h \
    mythread.h \
    my_main_qlabel.h

FORMS    += mainwindow.ui

LIBS += -lopencv_core \
        -lopencv_imgproc \
        -lopencv_highgui \
        -lzmq

RESOURCES += \
    Obrazky.qrc
