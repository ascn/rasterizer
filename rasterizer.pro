TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    tiny_obj_loader.cc \
    mat4.cpp \
    vec4.cpp \
    ppm.c \
    main.cpp \
    camera.cpp

DISTFILES += \
    cube.obj \
    dodecahedron.obj \
    square.obj \
    square_big.obj \
    wahoo.obj \
    rasterizer.pro.user \
    camera.txt

HEADERS += \
    tiny_obj_loader.h \
    mat4.h \
    ppm.h \
    vec4.h \
    camera.h
