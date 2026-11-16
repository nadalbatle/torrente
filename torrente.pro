QT += core gui qml quick quickcontrols2 dbus network

CONFIG += c++17 release
TARGET = torrente
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/classifier.h \
    src/cli.h \
    src/daemon.h \
    src/filepicker.h \
    src/portalfilepicker.h \
    src/qbtclient.h \
    src/searchmodel.h \
    src/theme.h \
    src/torrentmodel.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/classifier.cpp \
    src/cli.cpp \
    src/daemon.cpp \
    src/portalfilepicker.cpp \
    src/qbtclient.cpp \
    src/searchmodel.cpp \
    src/theme.cpp \
    src/torrentmodel.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
