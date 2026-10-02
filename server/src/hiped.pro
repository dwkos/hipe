#-------------------------------------------------
#
# Project created by QtCreator 2015-09-04T14:33:09
#
#-------------------------------------------------

QT       += core gui
QT       += svg

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = hiped
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app


SOURCES += main.cpp \
    keylist.cpp \
    connection.cpp \
    connectionmanager.cpp \
    container.cpp \
    containerframe.cpp \
    containertoplevel.cpp \
    hipe_instruction.c \
    common.c \
    sanitation.cpp \
    instructionhandler.cpp \
    mousecursor.cpp


HEADERS += main.hpp \
    ExpArray.hh \
    keylist.h \
    common.h \
    connection.h \
    connectionmanager.h \
    container.h \
    containerframe.h \
    containertoplevel.h \
    hipe_instruction.h \
    sanitation.h \
    instructionhandler.h \
    mousecursor.h

QMAKE_CXXFLAGS += -std=c++17 -Ofast -pthread

#hipecore, Hipe's display engine. Its headers are included as <HipeCore/...>. If it was installed
#under a prefix other than /usr, tell qmake where: qmake HIPECORE_PREFIX=/usr/local ...
#(and HIPECORE_LIBDIR=<dir> if its libraries are not in <prefix>/lib).
isEmpty(HIPECORE_PREFIX): HIPECORE_PREFIX = /usr
!equals(HIPECORE_PREFIX, /usr) {
    INCLUDEPATH += $$HIPECORE_PREFIX/include
    isEmpty(HIPECORE_LIBDIR): HIPECORE_LIBDIR = $$HIPECORE_PREFIX/lib
}
!isEmpty(HIPECORE_LIBDIR): LIBS += -L$$HIPECORE_LIBDIR
LIBS += -pthread -lHipeCore -lHipeCoreWidgets


# --- header-dependency tracking for hipecore's public headers ---
# qmake's built-in #include scanner does not follow headers resolved from system
# include paths, so a change to the installed <HipeCore/qweb*.h> headers (e.g. a
# WebAction enum edit in hipecore) would not rebuild the objects that use them,
# leaving hiped linked against a stale ABI. This is what caused the "Select All
# pastes" bug - a stale sanitation.o holding pre-InspectElement-removal WebAction
# values. Declare the dependency explicitly for every C++ object: GNU make merges
# the prerequisites of like-named rules, so this augments qmake's generated
# compile rules rather than replacing them.
WEBKIT_HEADERS = $$files($$HIPECORE_PREFIX/include/HipeCore/qweb*.h)
for(src, SOURCES) {
    contains(src, .*\\.cpp$) {
        obj = $$replace($$list($$basename(src)), \\.cpp$, .o)
        rule = webkithdrdep_$$replace($$list($$basename(src)), \\.cpp$,)
        eval($${rule}.target = $$obj)
        eval($${rule}.depends = $$WEBKIT_HEADERS)
        QMAKE_EXTRA_TARGETS += $$rule
    }
}
