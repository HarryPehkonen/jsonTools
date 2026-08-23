cmake_minimum_required(VERSION 3.20)
project(jsonTools VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Install layout (bin/lib/include), driven by CMAKE_INSTALL_PREFIX.
# Single-user default:  cmake -S . -B build -DCMAKE_INSTALL_PREFIX=$HOME/.local
# System-wide:          cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr/local
include(GNUInstallDirs)

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
  src/args.cpp
  src/common.cpp
  src/copy.cpp
  src/errors.cpp
  src/filter.cpp
  src/from.cpp
  src/get.cpp
  src/keys.cpp
  src/len.cpp
  src/move.cpp
  src/paths.cpp
  src/remove.cpp
  src/select.cpp
  src/set.cpp
  src/sort.cpp
  src/type.cpp
  src/zip.cpp
)
target_include_directories(jt_core PUBLIC include)
target_link_libraries(jt_core PUBLIC JSOM::jsom)
target_compile_options(jt_core PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)

# --- The tools (one executable per source file) ---
# The binary is jt<Verb>; the source is src/jt_<verb>.cpp.
# Each tool is added as it is implemented.
foreach(tool IN ITEMS New From Set Move Copy Remove Get Select Zip Sort Filter
                     Len Type Keys)
  string(TOLOWER "${tool}" tool_file)
  add_executable(jt${tool} src/jt_${tool_file}.cpp)
  target_link_libraries(jt${tool} PRIVATE jt_core)
  target_compile_options(jt${tool} PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)
endforeach()

# --- Tests ---
option(JT_BUILD_TESTS "Build jsonTools tests" OFF)
if(JT_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# --- Install ---
# Binaries + the static library + headers, into a CMAKE_INSTALL_PREFIX-driven
# layout (single-user ~/.local or system-wide /usr/local).
install(TARGETS jt_core ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
foreach(tool IN ITEMS New From Set Move Copy Remove Get Select Zip Sort Filter
                     Len Type Keys)
  install(TARGETS jt${tool} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endforeach()
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
