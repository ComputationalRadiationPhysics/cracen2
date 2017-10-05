cmake_minimum_required(VERSION 3.7.2)

project(cracen2benchamrk)

message(STATUS "CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH}")

###############################################################################
# Dependencies
###############################################################################

if(NOT TARGET cracen2)

find_package(cracen2 REQUIRED)
message(STATUS "CRACEN2_DIR " ${CRACEN2_DIR})
set(LIBS ${LIBS} cracen2)

endif()

###############################################################################
# Executables
###############################################################################

message(STATUS "Benchmarks:")
file(GLOB_RECURSE benchmarks
	"${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
)
foreach(benchmark ${benchmarks})
get_filename_component(name ${benchmark} NAME_WE)
message(STATUS "Added benchmark ${name} to project.")

add_executable("${name}Benchmark" ${benchmark})
target_link_libraries("${name}Benchmark" ${LIBS})

endforeach()
