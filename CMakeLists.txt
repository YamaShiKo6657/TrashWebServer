cmake_minimum_required(VERSION 3.28)
project(TrashwebServer
    VERSION 0.0.1
    DESCRIPTION "Webserver"
    HOMEPAGE_URL "https://github.com/YamaShiKo6657/TrashWebServer"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

set(Buffer_Dir ${CMAKE_SOURCE_DIR}/src/buffer)
set(Log_Dir ${CMAKE_SOURCE_DIR}/src/log)
set(Pool_Dir ${CMAKE_SOURCE_DIR}/src/threadpool)

add_subdirectory(${CMAKE_SOURCE_DIR}/src)