cmake_minimum_required(VERSION 3.20)
project(SoftRasterRenderer LANGUAGES CXX)

add_executable(Soft-Raster-Renderer
    Renderer.cpp ObjLoader.cpp ToolFunc.cpp tgaimage.cpp)
target_compile_features(Soft-Raster-Renderer PRIVATE cxx_std_20)
set_target_properties(Soft-Raster-Renderer PROPERTIES
    CXX_EXTENSIONS OFF
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
if(MSVC)
    target_compile_options(Soft-Raster-Renderer PRIVATE /utf-8)
endif()

option(SOFT_RASTER_OPENMP "Parallelize SSAO with OpenMP when available" ON)
if(SOFT_RASTER_OPENMP)
    find_package(OpenMP QUIET)
    if(OpenMP_CXX_FOUND)
        target_link_libraries(Soft-Raster-Renderer PRIVATE OpenMP::OpenMP_CXX)
    endif()
endif()
