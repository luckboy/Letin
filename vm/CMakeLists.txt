include_directories(..)

aux_source_directory("${CMAKE_CURRENT_SOURCE_DIR}" vm_sources)

add_library(letinvm ${vm_sources})
