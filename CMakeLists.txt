cmake_minimum_required(VERSION 3.10)
project(script)

set(include_dirs ${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${include_dirs})

option(RS_BIG_INTS "Use 64-bit script integers")
option(RS_BIG_FLOATS "Use 64-bit script floats")

file(GLOB src
	"src/*.h"
	"src/*.cpp"
)
source_group(src FILES ${src})

add_executable(script ${src})

set(defines )

if (RS_BIG_INTS)
	target_compile_definitions(script PRIVATE "SCRIPTS_USE_64BIT_INTEGERS=1")
endif ()

if (RS_BIG_FLOATS)
	target_compile_definitions(script PRIVATE "SCRIPTS_USE_64BIT_DECIMALS=1")
endif ()

target_link_libraries(script)

set_target_properties(script PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_CURRENT_SOURCE_DIR}/bin 
  RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_SOURCE_DIR}/bin
)
