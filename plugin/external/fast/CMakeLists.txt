cmake_minimum_required(VERSION 3.14...3.30)

# ---- Project ----
project(fast VERSION 0.2.0 LANGUAGES CXX)

# ---- Include guards ----
if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(
    FATAL_ERROR
      "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
  )
endif()

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# ---- Add source files ----
file(GLOB_RECURSE headers CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")
file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp")

# ---- Create library ----
add_library(${PROJECT_NAME} SHARED ${headers} ${sources})
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 20)
target_compile_definitions(${PROJECT_NAME} PUBLIC -DSLEEF_STATIC_LIBS=1 -DLLFIO_HEADERS_ONLY=0 -DLLFIO_STATIC_LINK=1)

# being a cross-platform target, we enforce standards conformance on MSVC
target_compile_options(${PROJECT_NAME} PUBLIC "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/permissive->")
target_compile_options(${PROJECT_NAME} PUBLIC "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/J>")
target_compile_options(${PROJECT_NAME} PUBLIC "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/GF>")

# All we need is threads
find_package(Threads REQUIRED)

# download CPM.cmake
file(
  DOWNLOAD
  https://github.com/cpm-cmake/CPM.cmake/releases/download/v0.38.3/CPM.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake
  EXPECTED_HASH SHA256=cc155ce02e7945e7b8967ddfaff0b050e958a723ef7aad3766d368940cb15494
)
include(${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake)

CPMAddPackage(
  NAME sleef
  VERSION 3.7.0
  GITHUB_REPOSITORY "shibatch/sleef"
  GIT_TAG 3.7.0
  OPTIONS
    "SLEEF_BUILD_SHARED_LIBS OFF"
    "SLEEF_BUILD_STATIC_TEST_BINS OFF"
    "SLEEF_ENABLE_LTO ON"
    "SLEEF_BUILD_LIBM ON"
    "SLEEF_BUILD_DFT OFF"
    "SLEEF_BUILD_QUAD OFF"
    "SLEEF_BUILD_GNUABI_LIBS OFF"
    "SLEEF_BUILD_SCALAR_LIB OFF"
    "SLEEF_BUILD_TESTS OFF"
    "SLEEF_ENABLE_CXX ON"
)
target_include_directories(${PROJECT_NAME} PRIVATE ${sleef_BINARY_DIR}/include)
target_link_libraries(${PROJECT_NAME} PRIVATE sleef)
CPMAddPackage("gh:eliaskosunen/scnlib@3.0.1")
target_link_libraries(${PROJECT_NAME} PRIVATE scn::scn)
CPMAddPackage("gh:ned14/outcome@2.2.9")
CPMAddPackage(
    name llfio
    VERSION 20240823
    GITHUB_REPOSITORY "ned14/llfio"
    GIT_TAG 20240823
    OPTIONS
        "BUILD_SHARED_LIBS OFF"
        "LLFIO_USE_EXPERIMENTAL_SG14_STATUS_CODE ON"
)
target_link_libraries(${PROJECT_NAME} PUBLIC llfio::sl)
if(MSVC)
set(OpenMP_RUNTIME_MSVC "experimental" FORCE)
endif()
find_package(OpenMP)
if (OpenMP_FOUND)
target_compile_options(${PROJECT_NAME} PUBLIC ${OpenMP_CXX_FLAGS})
target_link_libraries(${PROJECT_NAME} PRIVATE OpenMP::OpenMP_CXX)
target_compile_definitions(${PROJECT_NAME} PRIVATE -DFAST_USE_OMP)
endif()

target_include_directories(
  ${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
                         $<INSTALL_INTERFACE:include/${PROJECT_NAME}-${PROJECT_VERSION}>
)
