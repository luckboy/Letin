include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories(..)

aux_source_directory("${CMAKE_CURRENT_SOURCE_DIR}" letinc_sources)

if(NOT LINK_STATIC)
	list(APPEND letinc_libraries letincomp)
else(NOT LINK_STATIC)
	list(APPEND letinc_libraries letincomp_static)
	if("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
		list(APPEND letinc_libraries "${ws2_library}")
	endif("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
endif(NOT LINK_STATIC)

add_executable(letinc "" ${letinc_sources})
target_link_libraries(letinc ${letinc_libraries})

install(TARGETS letinc DESTINATION bin)
