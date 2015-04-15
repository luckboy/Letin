include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories(..)

aux_source_directory("${CMAKE_CURRENT_SOURCE_DIR}" letinc_sources)

if(NOT LINK_STATIC)
	set(letincomp_name "${CMAKE_SHARED_LIBRARY_PREFIX}letincomp${CMAKE_SHARED_LIBRARY_SUFFIX}")
	set(comp_library  "${PROJECT_BINARY_DIR}/comp/${letincomp_name}")
	list(APPEND letinc_libraries "${comp_library}")
else(NOT LINK_STATIC)
	set(letincomp_static_name "${CMAKE_STATIC_LIBRARY_PREFIX}letincomp${CMAKE_STATIC_LIBRARY_SUFFIX}")
	set(comp_static_library "${PROJECT_BINARY_DIR}/comp/${letincomp_static_name}")
	list(APPEND letinc_libraries "${comp_static_library}")
	if("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
		list(APPEND letinc_libraries "${ws2_library}")
	endif("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
endif(NOT LINK_STATIC)

add_executable(letinc "" ${letinc_sources})
target_link_libraries(letinc ${letinc_libraries})

install(TARGETS letinc DESTINATION bin)
