include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories(..)

aux_source_directory("${CMAKE_CURRENT_SOURCE_DIR}" letinc_sources)

set(letincomp_name "${CMAKE_SHARED_LIBRARY_PREFIX}letincomp${CMAKE_SHARED_LIBRARY_SUFFIX}")
set(comp_library  "${PROJECT_BINARY_DIR}/comp/${letincomp_name}")
list(APPEND letinc_libraries "${comp_library}")

add_executable(letinc "" ${letinc_sources})
target_link_libraries(letinc ${letinc_libraries})

install(TARGETS letinc DESTINATION bin)
