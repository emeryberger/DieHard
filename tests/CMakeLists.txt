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

# Double-free test
add_executable(test_doublefree test_doublefree.cpp)
target_compile_features(test_doublefree PRIVATE cxx_std_14)

# Invalid-free test
add_executable(test_invalidfree test_invalidfree.cpp)
target_compile_features(test_invalidfree PRIVATE cxx_std_14)

# Threadtest benchmark (multi-threaded allocation stress test)
add_executable(threadtest threadtest.cpp)
target_compile_features(threadtest PRIVATE cxx_std_14)
set_target_properties(threadtest PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED ON
)
if(UNIX AND NOT APPLE)
    target_link_libraries(threadtest PRIVATE pthread)
endif()

# Register tests with CTest
enable_testing()

# GlobalFreePool unit test - runs directly
add_test(NAME test_globalfreepool COMMAND test_globalfreepool)

# Tests that require DieHard preload
if(APPLE)
    set(PRELOAD_ENV "DYLD_INSERT_LIBRARIES=$<TARGET_FILE:diehard>")
else()
    set(PRELOAD_ENV "LD_PRELOAD=$<TARGET_FILE:diehard>")
endif()

add_test(NAME test_remote_free_diehard
         COMMAND ${CMAKE_COMMAND} -E env ${PRELOAD_ENV}
                 $<TARGET_FILE:test_remote_free>)

add_test(NAME test_doublefree_diehard
         COMMAND ${CMAKE_COMMAND} -E env ${PRELOAD_ENV}
                 $<TARGET_FILE:test_doublefree>)

add_test(NAME test_invalidfree_diehard
         COMMAND ${CMAKE_COMMAND} -E env ${PRELOAD_ENV}
                 $<TARGET_FILE:test_invalidfree>)

# Threadtest with DieHard (quick sanity check: 2 threads, few iterations)
add_test(NAME threadtest_diehard
         COMMAND ${CMAKE_COMMAND} -E env ${PRELOAD_ENV}
                 $<TARGET_FILE:threadtest> 2 10 1000 0 8)

# Make tests depend on the library being built
add_dependencies(test_remote_free diehard)
add_dependencies(test_doublefree diehard)
add_dependencies(test_invalidfree diehard)
add_dependencies(threadtest diehard)
