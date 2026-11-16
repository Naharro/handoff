QT += core gui quick quickcontrols2 testlib dbus network
CONFIG += c++17 testcase
TARGET = handoff_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/backend.h ../src/filepicker.h ../src/receiver.h ../src/rsync.h ../src/theme.h ../src/translator.h
SOURCES += handoff_tests.cpp ../src/backend.cpp ../src/receiver.cpp ../src/rsync.cpp ../src/theme.cpp ../src/translator.cpp
DEFINES += SOURCE_DIRECTORY=\\\"$$PWD/../src\\\"
