cmake_minimum_required(VERSION 3.18)

project(yc VERSION 1.0)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(C_STANDARD 99)

find_library(libcsv csv REQUIRED)
find_library(libcurl curl REQUIRED)
find_package(SQLite3 REQUIRED VERSION 3.3)

add_executable(${PROJECT_NAME} 
	src/main.c
	src/valor.c
	src/download.c
	src/data.c
	src/db.c
)

target_include_directories(${PROJECT_NAME} PRIVATE include ${SQLite3_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE m ${libcsv} ${libcurl} pthread ${SQLite3_LIBRARIES})
