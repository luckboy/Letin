include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories(..)

aux_source_directory("${CMAKE_CURRENT_SOURCE_DIR}" letin_sources)
find_library(vm_library NAMES letinvm PATHS "${PROJECT_BINARY_DIR}/vm" NO_DEFAULT_PATH)
list(APPEND letin_libraries "${vm_library}")

add_executable(letin "" ${letin_sources})
target_link_libraries(letin ${letin_libraries})
