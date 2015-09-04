#SUBDIRS= thread utility task_service log net statistic

CXXFLAGS =-Wall -Werror -g -O2 -fPIC -Wno-unused-function

#plugindir = ../../lib
plugindir = ${exec_prefix}/lib

plugin_LTLIBRARIES = libzlcoro.la

## 安装根目录

## 安装库目录

## 安装头文件目录

libzlcoro_la_LIBFLAGS =
libzlcoro_la_LIBADD =

INCLUDES                   = -I../
libzlcoro_la_CPPFLAGS =-O2 -std=c++0x -fPIC -shared
libzlcoro_la_LDFLAGS = -lrt -ldl

libzlcoro_la_SOURCES = Coroutine.cpp Hooks.cpp
