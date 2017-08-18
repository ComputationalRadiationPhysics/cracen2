cmake_minimum_required(VERSION 3.8.0)

project(cracen2)# VERSION 0.1.0)

message(STATUS "CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH}")
set(CRACEN2_DIR ${CMAKE_CURRENT_SOURCE_DIR})
message(STATUS "CRACEN2_DIR " ${CRACEN2_DIR})

###############################################################################
# Compiler Flags
###############################################################################

if(CMAKE_BUILD_TYPE STREQUAL "Release")
	message(STATUS "Configuring build in Release mode.")
	message(STATUS "	Disable warnings.")
	message(STATUS "	Disable debug symbols")
	message(STATUS "	Disable extern templates")
	message(STATUS "	Enable optimisations")

else()
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra")
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wunreachable-code")
	add_definitions(-DCRACEN2_ENABLE_EXTERN_TEMPLATES)

	message(STATUS "Configuring build in Debug mode.")
	message(STATUS "	Enable warnings.")
	message(STATUS "	Enable debug symbols")
	message(STATUS "	Enable extern templates")
	message(STATUS "	Disable optimisations")

endif()

###############################################################################
# Dependencies
###############################################################################
include(cracen2Dependencies.cmake.in)

set(CRACEN2_INCLUDE_DIR ${CRACEN2_INCLUDE_DIR} ${CRACEN2_DIR}/include)

message(STATUS "CRACEN2_INCLUDE_DIR: ${CRACEN2_INCLUDE_DIR}")
message(STATUS "CRACEN2_LIBRARIES: ${CRACEN2_LIBRARIES}")

set (LIBS ${LIBS} ${CRACEN2_LIBRARIES})
include_directories(${CRACEN2_INCLUDE_DIR})

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
add_library(cracen2 SHARED ${libsources})
add_library(cracen2-static STATIC ${libsources})
set_target_properties(cracen2-static PROPERTIES OUTPUT_NAME cracen2)
target_link_libraries(cracen2 "${LIBS}")
target_link_libraries(cracen2-static "${LIBS}")
set (LIBS ${LIBS} cracen2)

###############################################################################
# Executables
###############################################################################

message(STATUS "Executables")
file(GLOB_RECURSE executables
	"${CMAKE_CURRENT_SOURCE_DIR}/executables/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/executables/*.cc"
)
foreach(exe ${executables})
get_filename_component(name ${exe} NAME_WE)
message(STATUS "Added executable ${name} to project.")

add_executable("${name}" ${exe})
target_link_libraries("${name}" ${LIBS})

endforeach()

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

add_executable("${name}Test" ${test})
target_link_libraries("${name}Test" ${LIBS})
add_test(NAME "${name}Test" COMMAND "${name}Test")
set_tests_properties("${name}Test" PROPERTIES TIMEOUT 5)

endforeach()

###############################################################################
# Install
###############################################################################

file(READ cracen2Dependencies.cmake.in dependencies)
configure_file(cracen2Config.cmake.in cracen2Config.cmake @ONLY)

install(TARGETS cracen2
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
)
install(TARGETS cracen2-static
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
)

install(DIRECTORY include/cracen2
	DESTINATION include
)

install(FILES ${CMAKE_BINARY_DIR}/cracen2Config.cmake
	DESTINATION lib/cmake/cracen2
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
