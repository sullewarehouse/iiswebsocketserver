# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#

cmake_minimum_required (VERSION 3.8)
project ("WebSocketEcho")

ADD_DEFINITIONS(-DUNICODE)
ADD_DEFINITIONS(-D_UNICODE)

add_library(WebSocketEcho SHARED "main.cpp" "main.def" "iiswebsocket.cpp" "iiswebsocket.h")
