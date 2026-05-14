cmake_minimum_required(VERSION 3.20)
project(lrsp_native LANGUAGES C CXX VERSION 0.1.0)

# Place artefacts in build/{bin,lib} so the ctypes loader can find the DLL
# without environment trickery.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Pull in mespprc_native (which already vendors HiGHS). This makes the
# `mespprc_native` and `highs` targets available to us without duplicating
# either build.
set(MESPPRC_NATIVE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../mespprc_native"
    CACHE PATH "Path to the sibling mespprc_native source tree")
add_subdirectory("${MESPPRC_NATIVE_DIR}" mespprc_native_build)

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Public C ABI declared in include/lrsp.h. Source files are added one phase
# at a time; the v1 skeleton has just the foundation.
add_library(lrsp_native SHARED
    src/arena.c
    src/api.c
    src/instance.c
    src/instance_io.c
    src/column.c
    src/duals.c
    src/master.c
    src/pricing_graph.c
    src/pricing.c
    src/singleton_warmstart.c
    src/column_generation.c
    src/solver.c
    src/results.c
)

target_include_directories(lrsp_native PUBLIC include)
target_include_directories(lrsp_native PRIVATE
    "${MESPPRC_NATIVE_DIR}/include"
    "${MESPPRC_NATIVE_DIR}/third_party/HiGHS/src"
    "${MESPPRC_NATIVE_DIR}/third_party/HiGHS/src/interfaces"
    "${CMAKE_BINARY_DIR}/mespprc_native_build/third_party/HiGHS"
)
target_compile_definitions(lrsp_native PRIVATE LRSP_BUILDING_DLL)

target_link_libraries(lrsp_native PRIVATE mespprc_native highs)
set_target_properties(lrsp_native PROPERTIES LINKER_LANGUAGE CXX)

# Foundation smoke test
add_executable(lrsp_csmoke tests/csmoke.c)
target_link_libraries(lrsp_csmoke PRIVATE lrsp_native)

# The data-model unit test pokes internal helpers directly (column build,
# signature, dedup), so it links against the same source files lrsp_native
# uses rather than going through the public ABI.
add_executable(lrsp_test_column
    tests/test_column.c
    src/arena.c
    src/column.c
)
target_include_directories(lrsp_test_column PRIVATE include src)

add_executable(lrsp_test_instance_io tests/test_instance_io.c)
target_link_libraries(lrsp_test_instance_io PRIVATE lrsp_native)

add_executable(run_lrsp examples/run_lrsp.c)
target_link_libraries(run_lrsp PRIVATE lrsp_native)

add_executable(compare_ip_dp examples/compare_ip_dp.c)
target_link_libraries(compare_ip_dp PRIVATE lrsp_native)

add_executable(lrsp_test_pricing_dump
    tests/test_pricing_dump.c
    src/arena.c
    src/column.c
    src/duals.c
    src/instance.c
    src/instance_io.c
    src/master.c
    src/pricing_graph.c
    src/pricing.c
    src/singleton_warmstart.c
)
target_include_directories(lrsp_test_pricing_dump PRIVATE
    include src
    "${MESPPRC_NATIVE_DIR}/include"
    "${MESPPRC_NATIVE_DIR}/third_party/HiGHS/src"
    "${MESPPRC_NATIVE_DIR}/third_party/HiGHS/src/interfaces"
    "${CMAKE_BINARY_DIR}/mespprc_native_build/third_party/HiGHS"
)
target_link_libraries(lrsp_test_pricing_dump PRIVATE mespprc_native highs)
set_target_properties(lrsp_test_pricing_dump PROPERTIES LINKER_LANGUAGE CXX)

# The master smoke test pokes lrsp_master_* internals so it links against
# the implementation directly, just like test_column.c.
add_executable(lrsp_test_master_smoke
    tests/test_master_smoke.c
    src/arena.c
    src/column.c
    src/duals.c
    src/instance.c
    src/master.c
)
target_include_directories(lrsp_test_master_smoke PRIVATE
    include src
    "${MESPPRC_NATIVE_DIR}/third_party/HiGHS/src"
    "${MESPPRC_NATIVE_DIR}/third_party/HiGHS/src/interfaces"
    "${CMAKE_BINARY_DIR}/mespprc_native_build/third_party/HiGHS"
)
target_link_libraries(lrsp_test_master_smoke PRIVATE highs)
set_target_properties(lrsp_test_master_smoke PROPERTIES LINKER_LANGUAGE CXX)
