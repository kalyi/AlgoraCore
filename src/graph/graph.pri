message("pri file being processed: $$PWD")

HEADERS += \
    $$PWD/graphartifact.h \
    $$PWD/graph.h \
    $$PWD/vertex.h \
    $$PWD/arc.h

SOURCES += \
    $$PWD/graph.cpp \
    $$PWD/vertex.cpp \
    $$PWD/arc.cpp \
    $$PWD/graphartifact.cpp
