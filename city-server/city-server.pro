# общие настройки
CONFIG += console
CONFIG += warn_on
CONFIG -= qt

# файлы проекта
HEADERS += \
	source/Connection.h \
	source/Position.h \
	source/Player.h \
	source/Level.h \
    source/Castle.h
SOURCES += \
	source/Connection.cpp \
	source/main.cpp \
	source/Position.cpp \
	source/Player.cpp \
	source/Level.cpp \
    source/Castle.cpp

# внешние библиотеки
LIBS += \
	-lboost_system \
	-lboost_program_options \
	-lboost_regex \
	-lboost_thread

# флаги компилятора
QMAKE_CXXFLAGS += -std=c++03 -pedantic -Wall -W -O2
