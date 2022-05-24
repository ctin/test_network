cmake_minimum_required ( VERSION 2.8 )

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
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/error_handler.c
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/wierd_strrev.c
)

set (HEADERS
${CMAKE_CURRENT_SOURCE_DIR}/utils/error_handler.h
${CMAKE_CURRENT_SOURCE_DIR}/utils/wierd_strrev.h
)

if(UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
endif()

add_executable (${ProjectId} ${SOURCES} ${HEADERS} )

target_include_directories(${ProjectId} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(${ProjectId} PRIVATE ev)
if(UNIX AND NOT APPLE)
    target_link_libraries(${ProjectId} PRIVATE Threads::Threads)
endif()
