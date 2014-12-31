include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories(..)

aux_source_directory("${CMAKE_CURRENT_SOURCE_DIR}" letin_sources)

set(letinvm_name "${CMAKE_SHARED_LIBRARY_PREFIX}letinvm${CMAKE_SHARED_LIBRARY_SUFFIX}")
set(vm_library "${PROJECT_BINARY_DIR}/vm/${letinvm_name}")
list(APPEND letin_libraries "${vm_library}")

add_executable(letin "" ${letin_sources})
target_link_libraries(letin ${letin_libraries})

install(TARGETS letin DESTINATION bin)
