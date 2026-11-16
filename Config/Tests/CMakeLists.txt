# Host unit tests for the platform-independent Core (MSVC on Windows, AppleClang on the macOS CI runner).
# CI: cmake -S Tests -B build/tests -DCMAKE_BUILD_TYPE=Debug
#     cmake --build build/tests --parallel
#     ctest --test-dir build/tests --output-on-failure
cmake_minimum_required(VERSION 3.20)
project(OSTCoreTests LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# Repository root (parent of Tests/). Overridable so the harness can be exercised from a copy.
set(OST_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." CACHE PATH "iOS repository root")
get_filename_component(OST_REPO_ROOT "${OST_REPO_ROOT}" ABSOLUTE)
set(OST_FIXTURE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Fixtures")

option(OST_TEST_BOOLEAN "Fetch Manifold v3.0.0 and compile Core/Geometry/boolean_ops.cpp into ost_core" OFF)
# Windows has no system zlib: Core/Import/CabExtractor.cpp needs one. Elsewhere the system zlib is used.
if(WIN32)
  set(_ost_fetch_zlib_default ON)
else()
  set(_ost_fetch_zlib_default OFF)
endif()
option(OST_FETCH_ZLIB "Download zlib v1.3.1 instead of using the system zlib" ${_ost_fetch_zlib_default})

enable_testing()

# ---------------------------------------------------------------------------------------------
# ost_core: platform-independent sources of the iOS app.
# Everything under Core/ (*.cpp, *.c incl. sqlite3.c and pugixml.cpp) + Rendering/MeshLoader.cpp.
# Add Apple-only / host-incompatible files to OST_CORE_EXCLUDE (and say why).
# ---------------------------------------------------------------------------------------------
file(GLOB_RECURSE OST_CORE_SOURCES CONFIGURE_DEPENDS
  "${OST_REPO_ROOT}/Core/*.cpp"
  "${OST_REPO_ROOT}/Core/*.c")
set(OST_CORE_EXCLUDE
  # Needs the Manifold library; compiled only when OST_TEST_BOOLEAN=ON (see below).
  "${OST_REPO_ROOT}/Core/Geometry/boolean_ops.cpp")
list(REMOVE_ITEM OST_CORE_SOURCES ${OST_CORE_EXCLUDE})
list(APPEND OST_CORE_SOURCES "${OST_REPO_ROOT}/Rendering/MeshLoader.cpp")

if(OST_TEST_BOOLEAN)
  list(APPEND OST_CORE_SOURCES "${OST_REPO_ROOT}/Core/Geometry/boolean_ops.cpp")
else()
  # Same symbols as boolean_ops.hpp; every call throws so a test that reaches it FAILS loudly.
  list(APPEND OST_CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Support/BooleanOpsStub.cpp")
endif()

add_library(ost_core STATIC ${OST_CORE_SOURCES})
target_include_directories(ost_core PUBLIC
  "${OST_REPO_ROOT}"
  "${OST_REPO_ROOT}/Core"
  "${OST_REPO_ROOT}/Rendering")
target_compile_definitions(ost_core PUBLIC OST_HAVE_BOOLEAN=$<BOOL:${OST_TEST_BOOLEAN}>)

# Third-party single-file sources: do not drown the log in their warnings.
if(MSVC)
  set(_ost_no_warn /w)
else()
  set(_ost_no_warn -w)
endif()
set_source_files_properties(
  "${OST_REPO_ROOT}/Core/Persistence/sqlite3.c"
  "${OST_REPO_ROOT}/Core/Import/pugixml.cpp"
  PROPERTIES COMPILE_OPTIONS "${_ost_no_warn}")

if(MSVC)
  # Test-side shims instead of editing iOS sources: UTF-8 sources like clang, no CRT deprecation noise.
  target_compile_options(ost_core PUBLIC /utf-8)
  target_compile_definitions(ost_core PUBLIC _CRT_SECURE_NO_WARNINGS _USE_MATH_DEFINES NOMINMAX)
endif()

find_package(Threads REQUIRED)
target_link_libraries(ost_core PUBLIC Threads::Threads ${CMAKE_DL_LIBS})

# ---- zlib (CabExtractor) ----
if(OST_FETCH_ZLIB)
  include(FetchContent)
  FetchContent_Declare(zlibsrc
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG v1.3.1
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR ost_no_cmake_build) # download only; compiled below (zlib's own CMake renames files in its source tree)
  FetchContent_MakeAvailable(zlibsrc)
  set(_zs "${zlibsrc_SOURCE_DIR}")
  add_library(ost_zlib STATIC
    "${_zs}/adler32.c" "${_zs}/compress.c" "${_zs}/crc32.c" "${_zs}/deflate.c" "${_zs}/infback.c"
    "${_zs}/inffast.c" "${_zs}/inflate.c" "${_zs}/inftrees.c" "${_zs}/trees.c" "${_zs}/uncompr.c"
    "${_zs}/zutil.c")
  target_include_directories(ost_zlib PUBLIC "${_zs}")
  if(MSVC)
    target_compile_definitions(ost_zlib PRIVATE _CRT_SECURE_NO_DEPRECATE _CRT_NONSTDC_NO_DEPRECATE)
  else()
    target_compile_definitions(ost_zlib PRIVATE Z_HAVE_UNISTD_H)
  endif()
  target_link_libraries(ost_core PUBLIC ost_zlib)
else()
  find_package(ZLIB REQUIRED)
  target_link_libraries(ost_core PUBLIC ZLIB::ZLIB)
endif()

# ---- optional Manifold (boolean ops). Same version/flags as .github/workflows/build-manifold-ios.yml ----
if(OST_TEST_BOOLEAN)
  include(FetchContent)
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  set(MANIFOLD_PAR OFF CACHE BOOL "" FORCE)
  set(MANIFOLD_CROSS_SECTION OFF CACHE BOOL "" FORCE)
  set(MANIFOLD_TEST OFF CACHE BOOL "" FORCE)
  set(MANIFOLD_CBIND OFF CACHE BOOL "" FORCE)
  set(MANIFOLD_PYBIND OFF CACHE BOOL "" FORCE)
  set(MANIFOLD_EXPORT OFF CACHE BOOL "" FORCE)
  set(MANIFOLD_DEBUG OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(manifold
    GIT_REPOSITORY https://github.com/elalish/manifold.git
    GIT_TAG v3.0.0
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(manifold)
  target_link_libraries(ost_core PUBLIC manifold)
endif()

# ---------------------------------------------------------------------------------------------
# Test support (TestHarness.hpp + main()).
# ---------------------------------------------------------------------------------------------
add_library(ost_test_support STATIC Support/TestMain.cpp)
target_include_directories(ost_test_support PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Support")
if(MSVC)
  target_compile_options(ost_test_support PUBLIC /utf-8)
else()
  target_compile_options(ost_test_support PRIVATE -Wall -Wextra)
endif()

# ---------------------------------------------------------------------------------------------
# One executable per area: Tests/<area>/test_*.cpp -> target test_<area> (+ ctest entry <area>).
# Optional Tests/<area>/area.cmake is included with OST_AREA, OST_AREA_DIR, OST_AREA_TARGET set,
# after the target exists (target_sources / target_compile_definitions / ...).
# ---------------------------------------------------------------------------------------------
function(ost_add_area area)
  set(dir "${CMAKE_CURRENT_SOURCE_DIR}/${area}")
  file(GLOB area_sources CONFIGURE_DEPENDS "${dir}/test_*.cpp")
  if(NOT area_sources)
    return()
  endif()
  set(target "test_${area}")
  add_executable(${target} ${area_sources})
  target_link_libraries(${target} PRIVATE ost_core ost_test_support)
  if(NOT MSVC)
    target_compile_options(${target} PRIVATE -Wall -Wextra)
  endif()
  target_compile_definitions(${target} PRIVATE OST_FIXTURE_DIR="${OST_FIXTURE_DIR}")
  set(OST_AREA "${area}")
  set(OST_AREA_DIR "${dir}")
  set(OST_AREA_TARGET "${target}")
  if(EXISTS "${dir}/area.cmake")
    include("${dir}/area.cmake")
  endif()
  add_test(NAME ${area} COMMAND ${target})
  message(STATUS "OST test area: ${area} -> ${target}")
endfunction()

file(GLOB _ost_entries LIST_DIRECTORIES true RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*")
list(SORT _ost_entries)
foreach(_entry IN LISTS _ost_entries)
  if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${_entry}" AND NOT _entry MATCHES "^(Support|Fixtures)$")
    ost_add_area("${_entry}")
  endif()
endforeach()
