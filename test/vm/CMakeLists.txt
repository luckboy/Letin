include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories(../../vm/alloc)
include_directories(../../vm/gc)
include_directories(../../vm/vm)
include_directories(../../vm)
include_directories(../..)

aux_source_directory("${CMAKE_CURRENT_SOURCE_DIR}" vm_test_sources)

set(letinvm_static_name "${CMAKE_STATIC_LIBRARY_PREFIX}letinvm${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(vm_static_library "${PROJECT_BINARY_DIR}/vm/letinvm_static_name")
list(APPEND vm_test_libraries "${vm_library}")
list(APPEND vm_test_libraries ${CPPUNIT_LIBRARIES})

add_executable(testvm "" ${vm_test_sources})
target_link_libraries(testvm ${vm_test_libraries})
add_test(vm_test "${CMAKE_CURRENT_BINARY_DIR}/testvm${CMAKE_EXECUTABLE_SUFFIX}")
