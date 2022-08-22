#-------------------------------------------------
#
# Project created by QtCreator 2019-08-07T09:16:36
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = V4L2VideoProcess
TEMPLATE = app


SOURCES += main.cpp\
    pixmapwidget.cpp \
    videodisplaywidget.cpp \
    v4l2capture.cpp

HEADERS  += pixmapwidget.h \
    videodisplaywidget.h \
    v4l2capture.h

FORMS    +=
