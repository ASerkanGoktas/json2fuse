cmake_minimum_required(VERSION 3.17)
project(json2fuse)

set(CMAKE_MODULE_PATH CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMakeModules")

find_package(cjson REQUIRED)

find_package (FUSE REQUIRED)
include_directories (${FUSE_INCLUDE_DIR})

add_definitions(-D_FILE_OFFSET_BITS=64)
add_executable(json2fuse ./fuseops.c)


target_link_libraries(json2fuse ${FUSE_LIBRARIES} cjson)
target_include_directories(json2fuse PUBLIC /usr/local/include)

install(TARGETS json2fuse DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES filesystem.json DESTINATION ${CMAKE_INSTALL_PREFIX})
