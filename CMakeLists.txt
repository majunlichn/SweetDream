cmake_minimum_required(VERSION 3.25)

# Setup vcpkg toolchain file automatically.
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE FILEPATH "vcpkg toolchain file" FORCE)
endif()
if(DEFINED CMAKE_TOOLCHAIN_FILE)
    cmake_path(NORMAL_PATH CMAKE_TOOLCHAIN_FILE)
endif()
message(STATUS "CMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}")

project(SweetDream VERSION 0.0.0 LANGUAGES CXX)

set(SD_PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

get_property(IS_MULTICONFIG_GENERATOR GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

if(IS_MULTICONFIG_GENERATOR)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/$<CONFIG>")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/$<CONFIG>")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
else()
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()

set(SWEETDREAM_SOURCES
    Source/SweetDream/Core/Common/String.h
    Source/SweetDream/Core/IO/Logging.h
    Source/SweetDream/Core/IO/Logging.cpp
    Source/SweetDream/Core/API/Vulkan/VulkanCommon.h
    Source/SweetDream/Core/API/Vulkan/VulkanCommon.cpp
    Source/SweetDream/Core/API/Vulkan/VulkanInstance.h
    Source/SweetDream/Core/API/Vulkan/VulkanInstance.cpp
    Source/SweetDream/Core/API/Vulkan/VulkanSurface.h
    Source/SweetDream/Core/API/Vulkan/VulkanSurface.cpp
    Source/SweetDream/Core/API/Vulkan/VulkanSwapchain.h
    Source/SweetDream/Core/API/Vulkan/VulkanSwapchain.cpp
    Source/SweetDream/Core/API/Vulkan/VulkanDevice.h
    Source/SweetDream/Core/API/Vulkan/VulkanDevice.cpp
    Source/SweetDream/Core/API/Vulkan/VulkanQueue.h
    Source/SweetDream/Core/API/Vulkan/VulkanQueue.cpp
    Source/SweetDream/Core/API/Vulkan/VulkanBuffer.h
    Source/SweetDream/Core/API/Vulkan/VulkanBuffer.cpp
    Source/SweetDream/Core/API/Vulkan/VulkanImage.h
    Source/SweetDream/Core/API/Vulkan/VulkanImage.cpp
    Source/SweetDream/Core/API/Vulkan/VulkanSampler.h
    Source/SweetDream/Core/API/Vulkan/VulkanSampler.cpp
    Source/SweetDream/Core/API/Vulkan/VulkanCommand.h
    Source/SweetDream/Core/API/Vulkan/VulkanCommand.cpp
    Source/SweetDream/Core/API/Vulkan/VulkanUploader.h
    Source/SweetDream/Core/API/Vulkan/VulkanUploader.cpp
    Source/SweetDream/Core/API/Vulkan/VulkanFence.h
    Source/SweetDream/Core/API/Vulkan/VulkanFence.cpp
    Source/SweetDream/Core/API/Vulkan/VulkanSemaphore.h
    Source/SweetDream/Core/API/Vulkan/VulkanSemaphore.cpp
    Source/SweetDream/Core/API/Vulkan/VulkanEvent.h
    Source/SweetDream/Core/API/Vulkan/VulkanEvent.cpp
    Source/SweetDream/Core/API/Vulkan/VulkanQuery.h
    Source/SweetDream/Core/API/Vulkan/VulkanQuery.cpp
    Source/SweetDream/Core/API/Vulkan/VulkanRenderPass.h
    Source/SweetDream/Core/API/Vulkan/VulkanRenderPass.cpp
    Source/SweetDream/Core/API/Vulkan/VulkanFramebuffer.h
    Source/SweetDream/Core/API/Vulkan/VulkanFramebuffer.cpp
    Source/SweetDream/Core/API/Vulkan/VulkanPipeline.h
    Source/SweetDream/Core/API/Vulkan/VulkanPipeline.cpp
    Source/SweetDream/Core/API/Vulkan/VulkanDescriptor.h
    Source/SweetDream/Core/API/Vulkan/VulkanDescriptor.cpp
)

set(RAD_BUILD_TESTS OFF CACHE BOOL "Build radcpp tests" FORCE)
set(RAD_ENABLE_INSTALL OFF CACHE BOOL "Enable radcpp installation rules" FORCE)
add_subdirectory(ThirdParty/radcpp)

add_library(SweetDream STATIC)
add_library(SweetDream::SweetDream ALIAS SweetDream)

target_sources(SweetDream PRIVATE ${SWEETDREAM_SOURCES})
source_group(
    TREE "${CMAKE_CURRENT_SOURCE_DIR}/Source/SweetDream"
    FILES ${SWEETDREAM_SOURCES}
)

find_package(Vulkan REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)

target_include_directories(SweetDream
    PUBLIC
        "${SD_PROJECT_ROOT}/Source"
)

target_link_libraries(SweetDream
    PUBLIC Vulkan::Vulkan
    PUBLIC GPUOpen::VulkanMemoryAllocator
    PUBLIC rad::rad
)

if(WIN32)
    target_compile_definitions(SweetDream
        PUBLIC NOMINMAX=1
        PUBLIC WIN32_LEAN_AND_MEAN=1
    )
    target_compile_options(SweetDream
        PUBLIC "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
        PUBLIC "$<$<CXX_COMPILER_ID:MSVC>:/Zc:preprocessor>"
        PUBLIC "$<$<CXX_COMPILER_ID:MSVC>:/permissive->"
        PUBLIC "$<$<CXX_COMPILER_ID:MSVC>:/MP>"
    )
endif()

option(SD_BUILD_SAMPLES "Build SweetDream samples" ON)

if(SD_BUILD_SAMPLES)
    add_executable(HelloTriangle
        Samples/HelloTriangle/HelloTriangle.cpp
    )
    target_link_libraries(HelloTriangle
        PRIVATE SweetDream::SweetDream
    )
    set_target_properties(HelloTriangle PROPERTIES
        FOLDER Samples
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
    )
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
