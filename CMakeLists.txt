#
# Copyright (c) 2014-2021, NVIDIA CORPORATION. All rights reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.


cmake_minimum_required(VERSION 3.18)

project(Donut_MyDevs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

if (MSVC)
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /D_ITERATOR_DEBUG_LEVEL=1")
endif()

option(DONUT_WITH_ASSIMP "" OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
set(DONUT_SHADERS_OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/framework")

add_subdirectory(donut)
add_subdirectory(feature_demo)
add_subdirectory(examples/basic_triangle)
add_subdirectory(examples/vertex_buffer)
add_subdirectory(examples/deferred_shading)
add_subdirectory(examples/headless)
add_subdirectory(MyDevs/initial_project)
add_subdirectory(MyDevs/playground)
add_subdirectory(MyDevs/geometry_pipeline)

if (NVRHI_WITH_DX12)
    execute_process(COMMAND ${DXC_PATH} "-T cs_6_8" "ver_test.hlsl" OUTPUT_QUIET RESULT_VARIABLE DXC_SM_6_8 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/examples/work_graphs")
    if (DXC_SM_6_8 EQUAL "0")
        add_subdirectory(examples/work_graphs)
    else()
        message(WARNING "Work Graphs sample requires DirectX Compiler (dxc.exe) with Shader Model 6.8 support (dxcompiler.dll version 1.8.2403.34 or later) - sample will be omitted")
    endif()
endif()

if (NVRHI_WITH_VULKAN OR NVRHI_WITH_DX12)
    add_subdirectory(examples/bindless_rendering)
    add_subdirectory(examples/variable_shading)
    add_subdirectory(examples/rt_triangle)
    add_subdirectory(examples/rt_shadows)
    add_subdirectory(examples/rt_bindless)
    add_subdirectory(examples/rt_particles)
    add_subdirectory(examples/meshlets)

    if (DONUT_WITH_TASKFLOW)
        add_subdirectory(examples/threaded_rendering)
    endif()
endif()

if (NVRHI_WITH_DX12)
    # The RT Reflections example uses local bindings (local root signatures) which are unsupported on Vulkan
    add_subdirectory(examples/rt_reflections)
endif()

if (NVRHI_WITH_VULKAN)
    # Shader specializations are a unique Vulkan feature
    add_subdirectory(examples/shader_specializations)
endif()

if (DONUT_WITH_AFTERMATH AND NVRHI_WITH_AFTERMATH)
    add_subdirectory(examples/aftermath)
endif()
