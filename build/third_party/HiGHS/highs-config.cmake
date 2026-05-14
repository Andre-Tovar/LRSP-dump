

set(HIGHS_DIR "")

if(NOT TARGET highs)
  include("${CMAKE_CURRENT_LIST_DIR}/highs-targets.cmake")
endif()

set(HIGHS_LIBRARIES highs)

set(HIGHS_INCLUDE_DIRS "C:/Users/2027a/Desktop/LRSP-MESPPRC-IP/LRSP-MESPPRC-IP/mespprc_native/third_party/HiGHS/src;C:/Users/2027a/Desktop/LRSP-MESPPRC-IP/LRSP-MESPPRC-IP/build/third_party/HiGHS")

set(HIGHS_FOUND TRUE)

include(CMakeFindDependencyMacro)
find_dependency(Threads)
