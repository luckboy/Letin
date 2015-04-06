include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories("${CMAKE_CURRENT_BINARY_DIR}")
include_directories(..)

find_package(FLEX REQUIRED)
find_package(BISON REQUIRED)

include_directories("${FLEX_INCLUDE_DIR}")

flex_target(lexer lexer.lpp "${CMAKE_CURRENT_BINARY_DIR}/lexer.cpp")
bison_target(parser parser.ypp "${CMAKE_CURRENT_BINARY_DIR}/parser.cpp" COMPILE_FLAGS "-L c++ -r all")
add_flex_bison_dependency(lexer parser)

list(APPEND comp_sources ${BISON_parser_OUTPUTS})
list(APPEND comp_sources ${FLEX_lexer_OUTPUTS})
aux_source_directory("${CMAKE_CURRENT_SOURCE_DIR}" comp_sources)

add_library(letincomp SHARED ${comp_sources})
add_dependencies(letincomp lexer parser)
if("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
	target_link_libraries(letincomp ws2_32)
endif("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
	
add_library(letincomp_static STATIC ${comp_sources})
add_dependencies(letincomp_static lexer parser)
set_target_properties(letincomp_static PROPERTIES OUTPUT_NAME letincomp)

install(TARGETS letincomp letincomp_static DESTINATION lib)
