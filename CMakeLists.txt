cmake_minimum_required(VERSION 3.20)
project(jsonTools VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Install layout (bin/lib/include), driven by CMAKE_INSTALL_PREFIX.
# Single-user default:  cmake -S . -B build -DCMAKE_INSTALL_PREFIX=$HOME/.local
# System-wide:          cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr/local
include(GNUInstallDirs)

# --- Fuzzing (libFuzzer) ---
# Declared up here because it also shapes how JSOM and every target below are
# compiled; the fuzz target itself comes after the tests.
option(JT_BUILD_FUZZING "Build the libFuzzer target (requires clang)" OFF)
if(JT_BUILD_FUZZING)
  if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(FATAL_ERROR
      "JT_BUILD_FUZZING=ON needs clang++ (-fsanitize=fuzzer); got ${CMAKE_CXX_COMPILER_ID}")
  endif()

  # Instrument the CODE UNDER TEST, not just the harness. libFuzzer's whole
  # engine is coverage feedback: with only fuzz/jt_fuzz.cpp instrumented it can
  # see nothing inside jt_core, so it cannot explore the pointer traversal at
  # all. `fuzzer-no-link` gives the coverage callbacks without libFuzzer's
  # main(), which is what lets the other executables in this build directory
  # (the jt* tools, JSOM's) still link — only fuzz_jt adds `-fsanitize=fuzzer`.
  # JSOM's translation units get the same treatment: they enter the build below
  # as a subdirectory, so these options apply to them too.
  set(JT_FUZZ_INSTRUMENT
    -fsanitize=fuzzer-no-link,address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
  add_compile_options(${JT_FUZZ_INSTRUMENT})
  add_link_options(${JT_FUZZ_INSTRUMENT})

  # JSOM declares its own build_fuzzer / fuzz_quick / fuzz / fuzz_long custom
  # targets whenever clang++ is installed, and add_subdirectory() makes those
  # names global — so in jsonTools' fuzz build they collide with ours (CMake
  # refuses the duplicate name), and `--target build_fuzzer` would have built
  # JSOM's harness into JSOM's source tree instead of ours. find_program() does
  # nothing when its result variable is already defined, so pinning that probe
  # empty turns JSOM's fuzzing block off in THIS build only: JSOM's own checkout
  # is untouched, and jsonTools' fuzz build has exactly one harness.
  set(CLANG_PLUS_PLUS "" CACHE INTERNAL "JSOM's clang++ probe, pinned empty by jsonTools' fuzz build")
endif()

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
  src/values.cpp
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
                     Len Type Keys Values)
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

# --- Fuzzing (libFuzzer) ---
# The harness over jsonTools' OWN parsing surface: RFC 6901 pointer strings,
# the traversal that turns one into a document position, the write verbs built
# on it, and the argv helpers every tool shares. JSOM fuzzes JSON text
# separately (JSOM/tests/fuzzer.cpp). The target is opt-in and clang-only —
# libFuzzer IS clang's runtime; the instrumentation options are set at the top
# of this file, where they can still reach JSOM's subdirectory.

# The harness also lives in the DEFAULT build, as a compile-only object
# library: the tidy stage lints what the compile database covers, and a source
# no build sees is a source no gate sees (tools/ci.sh's ci_sources() /
# ci_tidy_sources() read fuzz/*.cpp for the same reason). Nothing links this
# object; the real fuzz target is built by the fuzz stage with clang.
add_library(jt_fuzz_harness OBJECT fuzz/jt_fuzz.cpp)
target_link_libraries(jt_fuzz_harness PRIVATE jt_core)

if(JT_BUILD_FUZZING)
  # JSOM's CMakeLists forces CMAKE_BUILD_TYPE=Release on the whole tree (it enters
  # as a subdirectory and sets the cache with FORCE), which is what the fuzzer
  # wants for speed — but it also strips -g. Ask for debug info on the code under
  # test, so a finding's report names lines and not just addresses.
  target_compile_options(jt_core PRIVATE -g)
  if(TARGET jsom_lib)
    target_compile_options(jsom_lib PRIVATE -g)
  endif()
  add_executable(fuzz_jt fuzz/jt_fuzz.cpp)
  target_link_libraries(fuzz_jt PRIVATE jt_core)
  # Only this target takes the fuzzer runtime (its main()); everything else in
  # this build directory is instrumented but links normally.
  target_compile_options(fuzz_jt PRIVATE -fsanitize=fuzzer -g -O1
    -Wall -Wextra -Wpedantic -Werror)
  target_link_options(fuzz_jt PRIVATE -fsanitize=fuzzer)
  # One name for the harness: cmake --build <dir> --target build_fuzzer.
  add_custom_target(build_fuzzer DEPENDS fuzz_jt)
  set_target_properties(fuzz_jt PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()

# --- Install ---
# Binaries + the static library + headers, into a CMAKE_INSTALL_PREFIX-driven
# layout (single-user ~/.local or system-wide /usr/local).
install(TARGETS jt_core ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
foreach(tool IN ITEMS New From Set Move Copy Remove Get Select Zip Sort Filter
                     Len Type Keys Values)
  install(TARGETS jt${tool} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endforeach()
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
# --- CODING_STANDARDS.md: warnings as errors for jsonTools' own targets ---
target_compile_options(jt_core PRIVATE -Werror)
foreach(jt_tool IN ITEMS New From Set Move Copy Remove Get Select Zip Sort Filter
                     Len Type Keys Values)
    if(TARGET jt${jt_tool})
        target_compile_options(jt${jt_tool} PRIVATE -Werror)
    endif()
endforeach()
if(TARGET jt_tests)
    target_compile_options(jt_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
