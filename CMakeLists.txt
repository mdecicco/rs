cmake_minimum_required(VERSION 3.10)
project(script)

set(include_dirs ${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${include_dirs})


file(GLOB src
	"src/*.h"
	"src/*.cpp"
)
source_group(src FILES ${src})

add_executable(script ${src})
target_link_libraries(script)

set_target_properties(script PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_CURRENT_SOURCE_DIR}/bin 
  RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_SOURCE_DIR}/bin
)
