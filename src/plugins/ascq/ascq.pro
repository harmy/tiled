include(../plugin.pri)

DEFINES += ASCQ_LIBRARY

greaterThan(QT_MAJOR_VERSION, 4) {
    OTHER_FILES = plugin.json
}

HEADERS += \
    ascq_global.h \
    ascqplugin.h

SOURCES += \
    ascqplugin.cpp


