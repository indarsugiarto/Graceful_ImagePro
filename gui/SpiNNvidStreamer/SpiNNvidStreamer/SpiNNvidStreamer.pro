#-------------------------------------------------
#
# Project created by QtCreator 2016-06-21T18:01:39
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = SpiNNvidStreamer
TEMPLATE = app


SOURCES += main.cpp\
        vidstreamer.cpp \
    cdecoder.cpp \
    cscreen.cpp

HEADERS  += vidstreamer.h \
    cdecoder.h \
    viddef.h \
    cscreen.h

FORMS    += vidstreamer.ui

LIBS += -lavcodec \
        -lavformat \
        -lswresample \
        -lswscale \
        -lavutil

DISTFILES +=

#Jika dipakai di Fedora, lokasi ffmpeg ada di:
INCLUDEPATH += /usr/include/ffmpeg
#Jika dipakai di Ubuntu, yang di atas tidak diperlukan

