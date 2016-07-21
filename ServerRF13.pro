QT += core network sql
QT -= gui

CONFIG += c++11

TARGET = listener
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    workWithDevices.cpp

HEADERS +=  workWithDevices.h
