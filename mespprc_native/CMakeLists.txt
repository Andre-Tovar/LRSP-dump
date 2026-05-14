cmake_minimum_required(VERSION 3.20)
project(mespprc_native LANGUAGES C CXX VERSION 0.1.0)

# Place all build artefacts in a predictable location next to the python package
# so the ctypes loader can find the shared library without environment trickery.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default to Release if not specified — HiGHS performance depends on it.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Vendor HiGHS as a static C++ library. We use HiGHS through its C API
# (third_party/HiGHS/src/interfaces/highs_c_api.h) but the library is C++
# under the hood, so the top-level project must enable CXX.
option(MESPPRC_USE_HIGHS "Build with HiGHS-backed Phase 2 IP" ON)
if(MESPPRC_USE_HIGHS)
    set(FAST_BUILD ON CACHE BOOL "" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(ALL_TESTS OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(ZLIB OFF CACHE BOOL "" FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(EXP OFF CACHE BOOL "" FORCE)
    set(PYTHON_BUILD_SETUP OFF CACHE BOOL "" FORCE)
    set(FORTRAN OFF CACHE BOOL "" FORCE)
    set(CSHARP OFF CACHE BOOL "" FORCE)
    add_subdirectory(third_party/HiGHS EXCLUDE_FROM_ALL)
endif()

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Shared library that exports the public ABI declared in include/mespprc.h.
add_library(mespprc_native SHARED
    src/arena.c
    src/bitset.c
    src/instance.c
    src/phase1.c
    src/phase2_dp.c
    src/phase2_ip.c
    src/api.c
)

target_include_directories(mespprc_native PUBLIC include)
target_compile_definitions(mespprc_native PRIVATE MESPPRC_BUILDING_DLL)

if(MESPPRC_USE_HIGHS)
    target_compile_definitions(mespprc_native PRIVATE MESPPRC_HAVE_HIGHS=1)
    target_include_directories(mespprc_native PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/HiGHS/src
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/HiGHS/src/interfaces
        ${CMAKE_CURRENT_BINARY_DIR}/third_party/HiGHS
    )
    target_link_libraries(mespprc_native PRIVATE highs)
    set_target_properties(mespprc_native PROPERTIES LINKER_LANGUAGE CXX)
endif()

# Optional: tiny C-side smoke executable. Useful during foundation work to
# verify struct sizes and arena allocation without bringing Python into the loop.
add_executable(mespprc_csmoke tests/csmoke.c)
target_link_libraries(mespprc_csmoke PRIVATE mespprc_native)
