QT += core gui qml quick quickcontrols2 dbus network

CONFIG += c++17 release
TARGET = handoff
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/cli.h \
    src/filepicker.h \
    src/portalfilepicker.h \
    src/receiver.h \
    src/rsync.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/cli.cpp \
    src/portalfilepicker.cpp \
    src/receiver.cpp \
    src/rsync.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
