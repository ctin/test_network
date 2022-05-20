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
    ${CMAKE_CURRENT_SOURCE_DIR}/async_server/async_server.c
    ${CMAKE_CURRENT_SOURCE_DIR}/async_server/vec.c
    ${CMAKE_CURRENT_SOURCE_DIR}/event_loop/event_loop.c
    ${CMAKE_CURRENT_SOURCE_DIR}/event_loop/functions_queue.c
    ${CMAKE_CURRENT_SOURCE_DIR}/server_handler/server_handler.c
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/error_handler.c
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/realloc.c
)

set (HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/async_server/async_server.h
    ${CMAKE_CURRENT_SOURCE_DIR}/async_server/vec.h
    ${CMAKE_CURRENT_SOURCE_DIR}/event_loop/event_loop.h
    ${CMAKE_CURRENT_SOURCE_DIR}/event_loop/functions_queue.h
    ${CMAKE_CURRENT_SOURCE_DIR}/server_handler/server_handler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/error_handler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/realloc.h
)

if(UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
endif()

add_executable (${ProjectId} ${SOURCES} ${HEADERS} )

target_include_directories(${ProjectId} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} )
if(UNIX AND NOT APPLE)
    target_link_libraries(${ProjectId} PRIVATE Threads::Threads)
endif()
