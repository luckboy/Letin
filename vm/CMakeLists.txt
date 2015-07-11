include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories(..)

aux_source_directory(alloc vm_sources)
aux_source_directory(gc vm_sources)
aux_source_directory(strategy vm_sources)
aux_source_directory(vm vm_sources)
aux_source_directory("${CMAKE_CURRENT_SOURCE_DIR}" vm_sources)

add_library(letinvm SHARED ${vm_sources})
if("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
	target_link_libraries(letinvm "${ws2_library}")
endif("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")

add_library(letinvm_static STATIC ${vm_sources})
set_target_properties(letinvm_static PROPERTIES OUTPUT_NAME letinvm)

install(TARGETS letinvm letinvm_static DESTINATION lib)
