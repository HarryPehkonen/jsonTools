# Google Test — prefer a system install, fall back to FetchContent.
find_package(GTest QUIET)
if(GTest_FOUND)
  set(JT_GTEST_LIBS GTest::gtest GTest::gtest_main)
else()
  include(FetchContent)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
  )
  FetchContent_MakeAvailable(googletest)
  set(JT_GTEST_LIBS gtest gtest_main)
endif()

# One file per verb, added as each verb is implemented.
add_executable(jt_tests
  test_paths.cpp
  test_errors.cpp
  test_from.cpp
  test_set.cpp
  test_copy.cpp
  test_move.cpp
  test_remove.cpp
  test_get.cpp
  test_select.cpp
  test_zip.cpp
  test_sort.cpp
  test_filter.cpp
  test_len.cpp
  test_type.cpp
  test_keys.cpp
  test_values.cpp
)

target_link_libraries(jt_tests PRIVATE jt_core ${JT_GTEST_LIBS})
target_compile_options(jt_tests PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)

# The brief runs `./build/jt_tests`, so keep the binary at the build root.
set_target_properties(jt_tests PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

include(GoogleTest)
gtest_discover_tests(jt_tests)
