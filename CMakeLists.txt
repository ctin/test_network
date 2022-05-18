cmake_minimum_required(VERSION 3.0)

get_filename_component(ProjectId ${CMAKE_CURRENT_SOURCE_DIR} NAME)

if (WIN32)
set(ProjectId ${ProjectId}_win)
elseif(UNIX AND NOT APPLE)
set(ProjectId ${ProjectId}_linux)
elseif(UNIX)
set(ProjectId ${ProjectId}_osx)
endif ()

string(REPLACE " " "_" ProjectId ${ProjectId})
project(${ProjectId})

set(SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/event_loop/event_loop.c
    ${CMAKE_CURRENT_SOURCE_DIR}/event_loop/functions_queue.c
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/error_handler.c
)

set (HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/event_loop/event_loop.h
    ${CMAKE_CURRENT_SOURCE_DIR}/event_loop/functions_queue.h
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/error_handler.h
)

add_executable (${ProjectId} ${SOURCES} ${HEADERS} )

target_include_directories(${ProjectId} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} )
