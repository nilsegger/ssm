cmake_minimum_required(VERSION 3.18)

project(yc VERSION 1.0)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_library(libcsv csv REQUIRED)

add_executable(${PROJECT_NAME} 
	src/main.c
	src/valor.c
)

target_include_directories(${PROJECT_NAME} PRIVATE include)
target_link_libraries(${PROJECT_NAME} PRIVATE m ${libcsv})
