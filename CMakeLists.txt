project(letin)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread")

include_directories(include)

add_subdirectory(vm)

if(BUILD_TESTING)
	enable_testing()

	find_package(CPPUNIT REQUIRED)

	add_subdirectory(test)
endif(BUILD_TESTING)
