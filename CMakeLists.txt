cmake_minimum_required(VERSION 3.8.0)
project(hl)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(LIBS_FOLDER "libs")

file(GLOB HL_SRC
    src/hl/*.cpp
    src/hl/*.h
)

add_library(${PROJECT_NAME} STATIC ${HL_SRC})
target_include_directories(${PROJECT_NAME} PUBLIC src)

# bzip2

file(GLOB BZIP2_SRC
    libs/bzip2/*.c
    libs/bzip2/*.h
)

target_include_directories(${PROJECT_NAME} PUBLIC libs/bzip2)
add_library(bzip2 STATIC ${BZIP2_SRC})
target_link_libraries(${PROJECT_NAME} bzip2)
set_property(TARGET bzip2 PROPERTY FOLDER ${LIBS_FOLDER})

# lua

file(GLOB LUA_SRC
	libs/lua/src/*.c
	libs/lua/src/*.h
)
	
target_include_directories(${PROJECT_NAME} PUBLIC libs/lua/src)
add_library(lua STATIC ${LUA_SRC})
target_link_libraries(${PROJECT_NAME} lua)
set_property(TARGET lua PROPERTY FOLDER ${LIBS_FOLDER})

# sol

target_include_directories(${PROJECT_NAME} PUBLIC libs/sol)

# sky

target_link_libraries(${PROJECT_NAME} sky)