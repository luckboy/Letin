include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories(../../comp)
include_directories(../..)

aux_source_directory("${CMAKE_CURRENT_SOURCE_DIR}" comp_test_sources)

find_library(comp_library NAMES letincomp PATHS "${PROJECT_BINARY_DIR}/comp" NO_DEFAULT_PATH)
list(APPEND comp_test_libraries "${comp_library}")
list(APPEND comp_test_libraries ${CPPUNIT_LIBRARIES})

add_executable(testcomp "" ${comp_test_sources})
target_link_libraries(testcomp ${comp_test_libraries})
add_test(comp_test "${CMAKE_CURRENT_BINARY_DIR}/testcomp${CMAKE_EXECUTABLE_SUFFIX}")
