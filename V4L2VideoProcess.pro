#-------------------------------------------------
#
# Project created by QtCreator 2019-08-07T09:16:36
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = V4L2VideoProcess

#指定编译中间文件的目录
MOC_DIR = ./build
OBJECTS_DIR = ./build
UI_DIR = ./build
RCC_DIR = ./build

#如需编译成库,则对应打开下面的语句
#TARGET = v4l2capture
#TEMPLATE = lib
#DESTDIR = ./libs
#CONFIG += staticlib
#QMAKE_POST_LINK += cp v4l2capture.h ./libs/

SOURCES += v4l2capture.cpp \
    colortorgb24.cpp

HEADERS  += v4l2capture.h \
    colortorgb24.h

if(contains(TEMPLATE,app)){
SOURCES += \
    main.cpp \
    pixmapwidget.cpp \
    videodisplaywidget.cpp

HEADERS += \
    pixmapwidget.h \
    videodisplaywidget.h
}
