include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories(..)

aux_source_directory("${CMAKE_CURRENT_SOURCE_DIR}" letin_sources)

if(NOT LINK_STATIC)
	set(letinvm_name "${CMAKE_SHARED_LIBRARY_PREFIX}letinvm${CMAKE_SHARED_LIBRARY_SUFFIX}")
	set(vm_library "${PROJECT_BINARY_DIR}/vm/${letinvm_name}")
	list(APPEND letin_libraries "${vm_library}")
else(NOT LINK_STATIC)
	set(letinvm_static_name "${CMAKE_STATIC_LIBRARY_PREFIX}letinvm${CMAKE_STATIC_LIBRARY_SUFFIX}")
	set(vm_static_library "${PROJECT_BINARY_DIR}/vm/${letinvm_static_name}")
	list(APPEND letin_libraries "${vm_static_library}")
	if("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
		list(APPEND letin_libraries "${ws2_library}")
	endif("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
endif(NOT LINK_STATIC)

add_executable(letin "" ${letin_sources})
target_link_libraries(letin ${letin_libraries})

install(TARGETS letin DESTINATION bin)
