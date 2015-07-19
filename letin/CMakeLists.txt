include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories(..)

aux_source_directory("${CMAKE_CURRENT_SOURCE_DIR}" letin_sources)

if(NOT LINK_STATIC)
	list(APPEND letin_libraries letinvm)
else(NOT LINK_STATIC)
	list(APPEND letin_libraries letinvm_static)
	if("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
		list(APPEND letin_libraries "${ws2_library}")
	endif("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
endif(NOT LINK_STATIC)

add_executable(letin "" ${letin_sources})
target_link_libraries(letin ${letin_libraries})

install(TARGETS letin DESTINATION bin)
