QT += core sql testlib qml
QT -= gui

CONFIG += c++17 console testcase
CONFIG -= app_bundle

TEMPLATE = app

macx {
    LIBS += -framework Security -framework LocalAuthentication -framework CoreFoundation
    INCLUDEPATH += $$PWD/../../libs/mac/include \
                   $$PWD/../../libs/mac/include/SDL2
    LIBS += -L$$PWD/../../libs/mac/lib -lSDL2 -lssl.3 -lcrypto.3
    OBJECTIVE_SOURCES += $$PWD/../../app/core/credentialstore_darwin.mm
}
win32 {
    LIBS += advapi32.lib
    contains(QT_ARCH, arm64) {
        INCLUDEPATH += $$PWD/../../libs/windows/include/arm64 \
                       $$PWD/../../libs/windows/include/arm64/SDL2
        LIBS += -L$$PWD/../../libs/windows/lib/arm64
    } else {
        INCLUDEPATH += $$PWD/../../libs/windows/include/x64 \
                       $$PWD/../../libs/windows/include/x64/SDL2
        LIBS += -L$$PWD/../../libs/windows/lib/x64
    }
    LIBS += -lSDL2
    LIBS += -llibssl -llibcrypto
}
unix:!macx {
    LIBS += -ldl
    CONFIG += link_pkgconfig
    PKGCONFIG += sdl2 openssl
}

INCLUDEPATH += $$PWD/../../app
INCLUDEPATH += $$PWD/../../moonlight-common-c/moonlight-common-c/src

SOURCES += \
    main.cpp \
    tst_settingsdatabase.cpp \
    tst_credentialstore.cpp \
    tst_controllermapstore.cpp \
    tst_hostcapabilities.cpp \
    tst_beaconspake2.cpp \
    tst_effectivesettingsresolver.cpp \
    $$PWD/../../app/core/settingsdatabase.cpp \
    $$PWD/../../app/core/credentialstore.cpp \
    $$PWD/../../app/backend/controllerprofilestore.cpp \
    $$PWD/../../app/backend/adapters/hostcapabilities.cpp \
    $$PWD/../../app/backend/beacon/spake2client.cpp \
    $$PWD/../../app/backend/beacon/beaconprotocol.cpp \
    $$PWD/../../app/settings/effectivesettingsresolver.cpp

HEADERS += \
    tst_settingsdatabase.h \
    tst_credentialstore.h \
    tst_controllermapstore.h \
    tst_hostcapabilities.h \
    tst_beaconspake2.h \
    tst_effectivesettingsresolver.h \
    $$PWD/../../app/core/settingsdatabase.h \
    $$PWD/../../app/core/credentialstore.h \
    $$PWD/../../app/backend/controllerprofilestore.h \
    $$PWD/../../app/backend/adapters/hostcapabilities.h \
    $$PWD/../../app/backend/beacon/spake2client.h \
    $$PWD/../../app/backend/beacon/beaconprotocol.h \
    $$PWD/../../app/settings/effectivesettingsresolver.h
