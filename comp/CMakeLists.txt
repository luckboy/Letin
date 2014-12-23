include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories("${CMAKE_CURRENT_BINARY_DIR}")
include_directories(..)

find_package(FLEX REQUIRED)
find_package(BISON REQUIRED)

flex_target(lexer lexer.lpp "${CMAKE_CURRENT_BINARY_DIR}/lexer.cpp")
bison_target(parser parser.ypp "${CMAKE_CURRENT_BINARY_DIR}/parser.cpp" COMPILE_FLAGS "-L c++ -Wconflicts-sr")
add_flex_bison_dependency(lexer parser)

list(APPEND comp_sources ${BISON_parser_OUTPUTS})
list(APPEND comp_sources ${FLEX_lexer_OUTPUTS})
aux_source_directory("${CMAKE_CURRENT_SOURCE_DIR}" comp_sources)

add_library(letincomp ${comp_sources})
add_dependencies(letincomp lexer parser)
