#-------------------------------------------------
#
# Project created by QtCreator 2016-10-19T21:57:09
#
#-------------------------------------------------

QT       += core gui
QT       += serialport

RC_ICONS = img\icon.ico

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = GM_SerialCom
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp

HEADERS  += mainwindow.h

FORMS    += \
    mainwindow.ui

RESOURCES += \
    resource.qrc
