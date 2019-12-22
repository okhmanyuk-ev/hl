file(GLOB HL_SRC
    "hl/src/hl/*.cpp"
    "hl/src/hl/*.h"
)

add_library(hl STATIC ${HL_SRC})
include_directories("hl/src")
target_link_libraries(${PROJECT_NAME} hl)

# bzip2

file(GLOB BZIP2_SRC
    "hl/libs/bzip2/*.c"
    "hl/libs/bzip2/*.h"
)

include_directories("hl/libs/bzip2")
add_library(bzip2 STATIC ${BZIP2_SRC})
target_link_libraries(${PROJECT_NAME} bzip2)

# lua

file(GLOB LUA_SRC
	"hl/libs/lua/src/*.c"
	"hl/libs/lua/src/*.h"
)
	
include_directories("hl/libs/lua/src")
add_library(lua STATIC ${LUA_SRC})
target_link_libraries(${PROJECT_NAME} lua)

# sol

include_directories("hl/libs/sol")