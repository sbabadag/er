QT       += core gui opengl openglwidgets widgets
CONFIG   += c++17

TEMPLATE = app
TARGET = OGL

# Directory structure
DESTDIR = $$PWD/bin
OBJECTS_DIR = $$PWD/build/obj
MOC_DIR = $$PWD/build/moc
UI_DIR = $$PWD/build/ui
RCC_DIR = $$PWD/build/rcc

# Include paths
INCLUDEPATH += $$PWD/include
QT_INCLUDE_PATH = ../../Qt/6.8.1/mingw_64
INCLUDEPATH += $$QT_INCLUDE_PATH/include
INCLUDEPATH += $$QT_INCLUDE_PATH/include/QtCore
INCLUDEPATH += $$QT_INCLUDE_PATH/include/QtGui
INCLUDEPATH += $$QT_INCLUDE_PATH/include/QtWidgets
INCLUDEPATH += $$QT_INCLUDE_PATH/include/QtOpenGL
INCLUDEPATH += $$QT_INCLUDE_PATH/include/QtOpenGLWidgets

# Library paths
LIBS += -L$$QT_INCLUDE_PATH/lib -lopengl32

HEADERS += \
    include/GLWidget.h \
    include/MainWindow.h \
    include/SnapManager.h \
    include/Line.h \
    include/DxfHandler.h \
    include/GhostTracker.h

SOURCES += \
    src/main.cpp \
    src/GLWidget.cpp \
    src/MainWindow.cpp \
    src/SnapManager.cpp \
    src/DxfHandler.cpp \
    src/GhostTracker.cpp

RC_ICONS = assets/appicon.ico

# Default rules for deployment
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target