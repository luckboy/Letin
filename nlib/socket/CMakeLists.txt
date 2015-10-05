include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

include(CheckTypeSize)

if("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
	set(CMAKE_EXTRA_INCLUDE_FILES "ws2tcpip.h")
	check_type_size("struct sockaddr_in6" STRUCT_SOCKADDR_IN6)
	set(CMAKE_EXTRA_INCLUDE_FILES)
else("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
	set(CMAKE_EXTRA_INCLUDE_FILES "netinet/in.h")
	check_type_size("struct sockaddr_in6" STRUCT_SOCKADDR_IN6)
	set(CMAKE_EXTRA_INCLUDE_FILES)
endif("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")

if(HAVE_STRUCT_SOCKADDR_IN6)
	add_definitions(-DHAVE_STRUCT_SOCKADDR_IN6)
endif(HAVE_STRUCT_SOCKADDR_IN6)

if(MINGW)
	add_definitions(-DWINVER=0x501)
endif(MINGW)

aux_source_directory("${CMAKE_CURRENT_SOURCE_DIR}" socket_sources)

add_library(socket MODULE ${socket_sources})
if("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
	target_link_libraries(socket letinvm)
endif("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")

install(TARGETS socket DESTINATION share/letin/nlib)
