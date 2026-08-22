cmake_minimum_required(VERSION 3.20)
project(jsonTools VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- JSOM dependency ---
# Prefer an existing source dir; otherwise FetchContent from GitHub.
set(JSOM_SOURCE_DIR "" CACHE PATH "Path to a local JSOM checkout")
if(JSOM_SOURCE_DIR)
  add_subdirectory("${JSOM_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/jsom")
else()
  include(FetchContent)
  FetchContent_Declare(
    jsom
    GIT_REPOSITORY https://github.com/HarryPehkonen/JSOM.git
    GIT_TAG        main
  )
  FetchContent_MakeAvailable(jsom)
endif()

# --- Shared core (libjt) ---
add_library(jt_core STATIC
  src/common.cpp
  src/errors.cpp
)
target_include_directories(jt_core PUBLIC include)
target_link_libraries(jt_core PUBLIC JSOM::jsom)

# --- The tools (one executable per source file) ---
# Each tool is added as it is implemented. Currently shipped: jtNew.
foreach(tool IN ITEMS new)
  add_executable(jt${tool} src/jt_${tool}.cpp)
  target_link_libraries(jt${tool} PRIVATE jt_core)
endforeach()

# --- Tests ---
option(JT_BUILD_TESTS "Build jsonTools tests" OFF)
if(JT_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
