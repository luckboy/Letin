include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

include(CheckTypeSize)

set(CMAKE_EXTRA_INCLUDE_FILES "netinet/in.h")
check_type_size("struct sockaddr_in6" STRUCT_SOCKADDR_IN6)
set(CMAKE_EXTRA_INCLUDE_FILES)

if(HAVE_STRUCT_SOCKADDR_IN6)
	add_definitions(-DHAVE_STRUCT_SOCKADDR_IN6)
endif(HAVE_STRUCT_SOCKADDR_IN6)

aux_source_directory("${CMAKE_CURRENT_SOURCE_DIR}" socket_sources)

add_library(socket MODULE ${socket_sources})

install(TARGETS socket DESTINATION share/letin/nlib)
