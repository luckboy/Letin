include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories(../../comp)
include_directories(../..)

aux_source_directory("${CMAKE_CURRENT_SOURCE_DIR}" comp_test_sources)

set(letincomp_static_name "${CMAKE_STATIC_LIBRARY_PREFIX}letincomp${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(comp_static_library "${PROJECT_BINARY_DIR}/comp/${letincomp_static_name}")
list(APPEND comp_test_libraries "${comp_library}")
list(APPEND comp_test_libraries ${CPPUNIT_LIBRARIES})

add_executable(testcomp "" ${comp_test_sources})
target_link_libraries(testcomp ${comp_test_libraries})
add_test(comp_test "${CMAKE_CURRENT_BINARY_DIR}/testcomp${CMAKE_EXECUTABLE_SUFFIX}")
