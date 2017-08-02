cmake_minimum_required(VERSION 3.0.2)
project(cracen2 VERSION 0.1 LANGUAGES CXX)

message(STATUS "CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH}")
set(cracen2_DIR ${CMAKE_CURRENT_SOURCE_DIR})
message(STATUS "cracen2_DIR " ${cracen2_DIR})
include_directories(${cracen2_DIR}/include/)

###############################################################################
# Compiler Flags
###############################################################################

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wunreachable-code")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

###############################################################################
# Find PThread
###############################################################################
ENABLE_LANGUAGE(C)
find_package(Threads REQUIRED)
set(LIBS ${LIBS} ${CMAKE_THREAD_LIBS_INIT})

###############################################################################
# Find Boost
###############################################################################
find_package(Boost 1.54.0 REQUIRED system)
include_directories(SYSTEM ${Boost_INCLUDE_DIRS})
set(LIBS ${LIBS} ${Boost_LIBRARIES})


###############################################################################
# Find CTest
###############################################################################

# include( CTest ) # add this for CI
enable_testing()

###############################################################################
# Cracen2 shared library object
###############################################################################

file(GLOB_RECURSE libsources
	"${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/source/*.cc"
)

message(STATUS ${libsources})
add_library(cracen2 STATIC ${libsources})
target_link_libraries(cracen2 "${LIBS}")
set (LIBS ${LIBS} cracen2)


###############################################################################
# Executables
###############################################################################

###############################################################################
# Tests
###############################################################################
message(STATUS "Test cases")
file(GLOB_RECURSE tests
	"${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/test/*.cc"
)
foreach(test ${tests})
get_filename_component(name ${test} NAME_WE)
message(STATUS "Added test ${name} to project.")

add_executable("${name}Test" ${SRCFILES} ${test})
target_link_libraries("${name}Test" ${LIBS})
add_test(NAME "${name}Test" COMMAND "${name}Test")

endforeach()

###############################################################################
# Install
###############################################################################

install(TARGETS cracen2 LIBRARY
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
)

install(DIRECTORY include
	DESTINATION include
)

###############################################################################
# Metrics
###############################################################################

EXEC_PROGRAM(
	"cloc ${CMAKE_CURRENT_SOURCE_DIR}/include/ /${CMAKE_CURRENT_SOURCE_DIR}/source/ ${CMAKE_CURRENT_SOURCE_DIR}/test/ ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt"
	OUTPUT_VARIABLE Metrics
	OUTPUT_QUIET
	ERROR_QUIET
	RETURN_VALUE MetricsReturn
)

if(${MetricsReturn} EQUAL 0)
	message(STATUS ${Metrics})
endif()

###############################################################################
# Documentation
###############################################################################

# add a target to generate API documentation with Doxygen
option(BUILD_DOCUMENTATION "Create and install the HTML based API documentation (requires Doxygen)" ${DOXYGEN_FOUND})

if(BUILD_DOCUMENTATION)
find_package(Doxygen)
 	if(DOXYGEN_FOUND)

	set(doxyfile_in ${CMAKE_CURRENT_SOURCE_DIR}/Doxygen.conf)
	set(doxyfile ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

    	configure_file(${doxyfile_in} ${doxyfile} @ONLY)

	add_custom_target(doc
        	COMMAND ${DOXYGEN_EXECUTABLE} ${doxyfile}
        	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        	COMMENT "Generating API documentation with Doxygen"
        	VERBATIM
 	)

	install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/html DESTINATION share/doc)

	endif()
endif()
