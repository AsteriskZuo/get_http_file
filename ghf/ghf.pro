#-------------------------------------------------
#
# Project created by QtCreator 2018-07-11T06:28:46
#
#-------------------------------------------------

QT       += core gui network sql

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = ghf
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    get_http_file.cpp

HEADERS  += mainwindow.h \
    get_http_file.h

FORMS    += mainwindow.ui
