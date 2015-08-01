#CXXFLAGS += -Wall -static -g -O2 -fPIC

bin_PROGRAMS = test1 test_pthread

INCLUDES = -I../src
CPPFLAGS = -std=c++0x -O2
LDFLAGS  = -O2 -L../lib -lzlcoro

test1_SOURCES          = test1.cpp

test_pthread_SOURCES  = test_pthread.cpp
