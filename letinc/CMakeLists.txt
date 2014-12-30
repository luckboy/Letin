include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories(..)

aux_source_directory("${CMAKE_CURRENT_SOURCE_DIR}" letinc_sources)
find_library(comp_library NAMES letincomp PATHS "${PROJECT_BINARY_DIR}/comp" NO_DEFAULT_PATH)
list(APPEND letinc_libraries "${comp_library}")

add_executable(letinc "" ${letinc_sources})
target_link_libraries(letinc ${letinc_libraries})
