# Tests for DieHard allocator

# Unit test for GlobalFreePool (doesn't require library preload)
add_executable(test_globalfreepool test_globalfreepool.cpp)
target_include_directories(test_globalfreepool PRIVATE ${CMAKE_SOURCE_DIR}/src/include)
target_compile_features(test_globalfreepool PRIVATE cxx_std_14)
set_target_properties(test_globalfreepool PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED ON
)
if(UNIX AND NOT APPLE)
    target_link_libraries(test_globalfreepool PRIVATE pthread)
endif()

# Cross-thread free test (requires library preload to test DieHard)
add_executable(test_remote_free test_remote_free.cpp)
target_compile_features(test_remote_free PRIVATE cxx_std_14)
set_target_properties(test_remote_free PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED ON
)
if(UNIX AND NOT APPLE)
    target_link_libraries(test_remote_free PRIVATE pthread)
endif()

# Register tests with CTest
enable_testing()

# GlobalFreePool unit test - runs directly
add_test(NAME test_globalfreepool COMMAND test_globalfreepool)

# Cross-thread free test with DieHard preload
if(APPLE)
    add_test(NAME test_remote_free_diehard
             COMMAND ${CMAKE_COMMAND} -E env
                     "DYLD_INSERT_LIBRARIES=$<TARGET_FILE:diehard>"
                     $<TARGET_FILE:test_remote_free>)
else()
    add_test(NAME test_remote_free_diehard
             COMMAND ${CMAKE_COMMAND} -E env
                     "LD_PRELOAD=$<TARGET_FILE:diehard>"
                     $<TARGET_FILE:test_remote_free>)
endif()

# Make tests depend on the library being built
add_dependencies(test_remote_free diehard)
