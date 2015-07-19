include_directories("${CPPUNIT_INCLUDE_DIR}")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories(../../vm/alloc)
include_directories(../../vm/gc)
include_directories(../../vm/vm)
include_directories(../../vm)
include_directories(../..)

aux_source_directory("${CMAKE_CURRENT_SOURCE_DIR}" vm_test_sources)

list(APPEND vm_test_libraries letinvm_static)
list(APPEND vm_test_libraries ${CPPUNIT_LIBRARIES})
if("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
	list(APPEND vm_test_libraries "${ws2_library}")
endif("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")

add_executable(testvm "" ${vm_test_sources})
target_link_libraries(testvm ${vm_test_libraries})
add_test(vm_test "${CMAKE_CURRENT_BINARY_DIR}/testvm${CMAKE_EXECUTABLE_SUFFIX}")
