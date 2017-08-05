# - Try to find cracen2
# Once done this will define
#
#  CRACEN2_DIR - base dir of cracen installation
#  CRACEN2_FOUND - system has cracen2
#  CRACEN2_INCLUDE_DIR - the cracen2 include directory
#  CRACEN2_LIBRARIES - Link these to use cracen2

cmake_minimum_required(VERSION 3.0.2)

project("cracen2")

# set(CRACEN2_VERSION 0.1.0)

if(${CRACEN2_DIR})
set(CRACEN2_DIR ${PROJECT_SOURCE_DIR})
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

###############################################################################
# Find PThread
###############################################################################
ENABLE_LANGUAGE(C)
find_package(Threads REQUIRED)
set(CRACEN2_LIBRARIES ${CRACEN2_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

###############################################################################
# Find Boost
###############################################################################
find_package(Boost 1.54.0 REQUIRED system)
set(CRACEN2_INCLUDE_DIR ${CRACEN2_INCLUDE_DIR} ${Boost_INCLUDE_DIRS})
set(CRACEN2_LIBRARIES ${CRACEN2_LIBRARIES} ${Boost_LIBRARIES})


###############################################################################
# Find Cracen2 Headers and Libraries
###############################################################################

find_library(CRACEN2_SHARED_LIB cracen2
	HINTS ${CRACEN_DIR}/lib
	PATH_SUFFIXES cracen2
)

set(CRACEN2_INCLUDE_DIR ${CRACEN2_DIR}/include)
if(${CRACEN2_SHARED_LIB})
	set(CRACEN2_LIBRARIES ${CRACEN2_LIBRARIES} ${CRACEN2_SHARED_LIB})
	set(CRACEN2_FOUND TRUE)
else()
	message(WARNING "Could not find the shared library of cracen2. This may be caused by a broken installation.")
	set(CRACEN2_FOUND FALSE)
endif()

