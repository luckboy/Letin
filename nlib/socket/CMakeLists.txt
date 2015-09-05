include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

aux_source_directory("${CMAKE_CURRENT_SOURCE_DIR}" socket_sources)

add_library(socket MODULE ${socket_sources})

install(TARGETS socket DESTINATION share/letin/nlib)
