# CMakeList.txt : Top-level CMake project file for WebSocketEcho

cmake_minimum_required (VERSION 3.8)
project ("WebSocketEcho")

ADD_DEFINITIONS(-DUNICODE)
ADD_DEFINITIONS(-D_UNICODE)

add_library(WebSocketEcho SHARED "example.cpp" "example.def" "iiswebsocket.cpp" "iiswebsocket.h")