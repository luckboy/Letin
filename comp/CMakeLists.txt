include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories(..)

aux_source_directory("${CMAKE_CURRENT_SOURCE_DIR}" comp_sources)

add_library(letincomp ${comp_sources})
