#CXXFLAGS += -Wall -static -g -O2 -fPIC

bin_PROGRAMS = test_simple test_pthread test_echoserver

INCLUDES = -I../src
CPPFLAGS = -std=c++0x -O2
LDFLAGS  = -O2 -L../lib -lzlcoro

test_simple_SOURCES          = test_simple.cpp

test_pthread_SOURCES         = test_pthread.cpp

test_echoserver_SOURCES      = test_echoserver.cpp
