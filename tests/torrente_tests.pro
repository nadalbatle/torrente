QT += core gui quick quickcontrols2 testlib dbus network
CONFIG += c++17 testcase
TARGET = torrente_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/classifier.h ../src/torrentmodel.h ../src/theme.h ../src/translator.h
SOURCES += torrente_tests.cpp ../src/classifier.cpp ../src/torrentmodel.cpp ../src/theme.cpp ../src/translator.cpp
