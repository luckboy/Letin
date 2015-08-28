include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

aux_source_directory("${CMAKE_CURRENT_SOURCE_DIR}" posix_sources)

add_library(posix MODULE ${posix_sources})

install(TARGETS posix DESTINATION share/letin/nlib)
