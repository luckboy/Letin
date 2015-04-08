project(letin)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

find_package(Threads REQUIRED)

if(CMAKE_COMPILER_IS_GNUCXX OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

	if(CMAKE_USE_PTHREADS_INIT)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
	endif(CMAKE_USE_PTHREADS_INIT)

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

	if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-register -Wno-array-bounds")
	endif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")

	if(MINGW)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -U__STRICT_ANSI__")
	endif(MINGW)
endif(CMAKE_COMPILER_IS_GNUCXX OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")

if("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
	set(ws2_library ws2_32)
	if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "8")
		set(ws2_library ws2_64)
	endif("${CMAKE_SIZEOF_VOID_P}" STREQUAL "8")
endif("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")

include_directories(include)

add_subdirectory(comp)
add_subdirectory(vm)
add_subdirectory(letinc)
add_subdirectory(letin)

install(DIRECTORY include/letin DESTINATION include)

if(BUILD_TESTING)
	enable_testing()

	find_package(CPPUNIT REQUIRED)

	add_subdirectory(test)
endif(BUILD_TESTING)
