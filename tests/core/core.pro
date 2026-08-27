QT += core sql testlib
QT -= gui

CONFIG += c++17 console testcase
CONFIG -= app_bundle

TEMPLATE = app

macx {
    LIBS += -framework Security -framework CoreFoundation
}
win32 {
    LIBS += advapi32.lib
}
unix:!macx {
    LIBS += -ldl
}

INCLUDEPATH += $$PWD/../../app

SOURCES += \
    main.cpp \
    tst_settingsdatabase.cpp \
    tst_credentialstore.cpp \
    $$PWD/../../app/core/settingsdatabase.cpp \
    $$PWD/../../app/core/credentialstore.cpp

HEADERS += \
    tst_settingsdatabase.h \
    tst_credentialstore.h \
    $$PWD/../../app/core/settingsdatabase.h \
    $$PWD/../../app/core/credentialstore.h
