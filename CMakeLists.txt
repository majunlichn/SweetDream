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
file(TO_CMAKE_PATH "$ENV{VULKAN_SDK}" SD_VULKAN_SDK_ROOT)

option(SD_BUILD_SAMPLES "Build SweetDream samples" ON)
option(SD_BUILD_GUI "Build SweetDream GUI support using SDL3" ON)
include(cmake/sdFindSDL3.cmake)

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
    Source/SweetDream/Shader/GLSLCompiler.h
    Source/SweetDream/Shader/GLSLCompiler.cpp
    Source/SweetDream/Shader/SpvContext.h
    Source/SweetDream/Shader/SpvContext.cpp
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

find_package(Vulkan REQUIRED COMPONENTS glslc)
find_library(SHADERC_SHARED_LIBRARY
    NAMES shaderc_shared
    HINTS "${SD_VULKAN_SDK_ROOT}/Lib" "${SD_VULKAN_SDK_ROOT}/lib"
    REQUIRED
)
add_library(shaderc::shaderc SHARED IMPORTED)

if(WIN32)
    find_file(SHADERC_SHARED_RUNTIME
        NAMES shaderc_shared.dll
        HINTS "${SD_VULKAN_SDK_ROOT}/Bin" "${SD_VULKAN_SDK_ROOT}/bin"
        REQUIRED
    )
    set_target_properties(shaderc::shaderc PROPERTIES
        IMPORTED_IMPLIB "${SHADERC_SHARED_LIBRARY}"
        IMPORTED_LOCATION "${SHADERC_SHARED_RUNTIME}"
    )
else()
    set_target_properties(shaderc::shaderc PROPERTIES
        IMPORTED_LOCATION "${SHADERC_SHARED_LIBRARY}"
    )
endif()
find_path(SPIRV_TOOLS_INCLUDE_DIR
    NAMES spirv-tools/libspirv.h
    HINTS "${SD_VULKAN_SDK_ROOT}/Include" "${SD_VULKAN_SDK_ROOT}/include"
    REQUIRED
)
find_library(SPIRV_TOOLS_SHARED_LIBRARY
    NAMES SPIRV-Tools-shared
    HINTS "${SD_VULKAN_SDK_ROOT}/Lib" "${SD_VULKAN_SDK_ROOT}/lib"
    REQUIRED
)
add_library(spirv-tools::spirv-tools SHARED IMPORTED)
set_target_properties(spirv-tools::spirv-tools PROPERTIES
    INTERFACE_COMPILE_DEFINITIONS SPIRV_TOOLS_SHAREDLIB
    INTERFACE_INCLUDE_DIRECTORIES "${SPIRV_TOOLS_INCLUDE_DIR}"
)
if(WIN32)
    find_file(SPIRV_TOOLS_SHARED_RUNTIME
        NAMES SPIRV-Tools-shared.dll
        HINTS "${SD_VULKAN_SDK_ROOT}/Bin" "${SD_VULKAN_SDK_ROOT}/bin"
        REQUIRED
    )
    set_target_properties(spirv-tools::spirv-tools PROPERTIES
        IMPORTED_IMPLIB "${SPIRV_TOOLS_SHARED_LIBRARY}"
        IMPORTED_LOCATION "${SPIRV_TOOLS_SHARED_RUNTIME}"
    )
else()
    set_target_properties(spirv-tools::spirv-tools PROPERTIES
        IMPORTED_LOCATION "${SPIRV_TOOLS_SHARED_LIBRARY}"
    )
endif()
find_package(VulkanMemoryAllocator CONFIG REQUIRED)

target_include_directories(SweetDream
    PUBLIC
        "${SD_PROJECT_ROOT}/Source"
)

target_link_libraries(SweetDream
    PUBLIC Vulkan::Vulkan
    PUBLIC GPUOpen::VulkanMemoryAllocator
    PUBLIC rad::rad
    PUBLIC shaderc::shaderc
    PUBLIC spirv-tools::spirv-tools
)

if(SD_BUILD_GUI)
    sd_find_sdl3()

    set(IMGUI_SOURCES
        ThirdParty/imgui/imgui.cpp
        ThirdParty/imgui/imgui_draw.cpp
        ThirdParty/imgui/imgui_tables.cpp
        ThirdParty/imgui/imgui_widgets.cpp
        ThirdParty/imgui/backends/imgui_impl_sdl3.cpp
        ThirdParty/imgui/backends/imgui_impl_vulkan.cpp
    )
    set(IMPLOT_SOURCES
        ThirdParty/implot/implot.cpp
        ThirdParty/implot/implot_items.cpp
    )

    # Keep GUI enabled by default without making SDL3 or the GUI submodules
    # mandatory for core-only configurations.
    set(SD_GUI_DEPENDENCIES_FOUND ON)
    if(NOT TARGET SDL3::SDL3)
        set(SD_GUI_DEPENDENCIES_FOUND OFF)
    endif()
    foreach(IMGUI_SOURCE IN LISTS IMGUI_SOURCES)
        if(NOT EXISTS "${SD_PROJECT_ROOT}/${IMGUI_SOURCE}")
            set(SD_GUI_DEPENDENCIES_FOUND OFF)
        endif()
    endforeach()
    foreach(IMPLOT_SOURCE IN LISTS IMPLOT_SOURCES)
        if(NOT EXISTS "${SD_PROJECT_ROOT}/${IMPLOT_SOURCE}")
            set(SD_GUI_DEPENDENCIES_FOUND OFF)
        endif()
    endforeach()

    if(NOT SD_GUI_DEPENDENCIES_FOUND)
        message(WARNING
            "SD_BUILD_GUI is enabled, but SDL3, Dear ImGui, or ImPlot was not found; "
            "GUI support will be disabled for this configuration."
        )
        set(SD_BUILD_GUI OFF)
    endif()
endif()

if(SD_BUILD_GUI)
    source_group(
        TREE "${SD_PROJECT_ROOT}/ThirdParty/imgui"
        FILES ${IMGUI_SOURCES}
    )
    add_library(imgui STATIC EXCLUDE_FROM_ALL ${IMGUI_SOURCES})
    add_library(imgui::imgui ALIAS imgui)
    target_include_directories(imgui SYSTEM
        PUBLIC
            "${SD_PROJECT_ROOT}/Source"
            "${SD_PROJECT_ROOT}/ThirdParty/imgui"
            "${SD_PROJECT_ROOT}/ThirdParty/imgui/backends"
    )
    target_compile_definitions(imgui PUBLIC
        IMGUI_USER_CONFIG=\"SweetDream/Gui/ImGuiConfig.h\"
    )
    target_link_libraries(imgui
        PUBLIC Vulkan::Vulkan
        PUBLIC SDL3::SDL3
    )
    set_target_properties(imgui PROPERTIES FOLDER ThirdParty)

    source_group(
        TREE "${SD_PROJECT_ROOT}/ThirdParty/implot"
        FILES ${IMPLOT_SOURCES}
    )
    add_library(implot STATIC EXCLUDE_FROM_ALL ${IMPLOT_SOURCES})
    add_library(implot::implot ALIAS implot)
    target_include_directories(implot SYSTEM
        PUBLIC "${SD_PROJECT_ROOT}/ThirdParty/implot"
    )
    target_link_libraries(implot PUBLIC imgui::imgui)
    set_target_properties(implot PROPERTIES FOLDER ThirdParty)

    set(SWEETDREAM_PLATFORM_SOURCES
        Source/SweetDream/Platform/EventHandler.h
        Source/SweetDream/Platform/EventHandler.cpp
        Source/SweetDream/Platform/Application.h
        Source/SweetDream/Platform/Application.cpp
    )
    set(SWEETDREAM_GUI_SOURCES
        Source/SweetDream/Gui/ImGuiConfig.h
        Source/SweetDream/Gui/Window.h
        Source/SweetDream/Gui/Window.cpp
        Source/SweetDream/Gui/VulkanWindow.h
        Source/SweetDream/Gui/VulkanWindow.cpp
        Source/SweetDream/Gui/GuiContext.h
        Source/SweetDream/Gui/GuiContext.cpp
    )
    target_sources(SweetDream PRIVATE
        ${SWEETDREAM_PLATFORM_SOURCES}
        ${SWEETDREAM_GUI_SOURCES}
    )
    source_group(
        TREE "${CMAKE_CURRENT_SOURCE_DIR}/Source/SweetDream"
        FILES
            ${SWEETDREAM_PLATFORM_SOURCES}
            ${SWEETDREAM_GUI_SOURCES}
    )

    target_link_libraries(SweetDream
        PUBLIC SDL3::SDL3
        PUBLIC imgui::imgui
        PUBLIC implot::implot
    )
    target_compile_definitions(SweetDream PUBLIC SD_BUILD_GUI=1)
endif()

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

if(SD_BUILD_SAMPLES AND SD_BUILD_GUI)
    add_executable(HelloTriangle
        Samples/HelloTriangle/HelloTriangle.cpp
        Samples/HelloTriangle/MainWindow.h
        Samples/HelloTriangle/MainWindow.cpp
        ThirdParty/imgui/imgui_demo.cpp
        ThirdParty/implot/implot_demo.cpp
    )
    target_link_libraries(HelloTriangle
        PRIVATE SweetDream::SweetDream
    )
    set_target_properties(HelloTriangle PROPERTIES
        FOLDER Samples
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
    )
    if(WIN32)
        add_custom_command(TARGET HelloTriangle POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:shaderc::shaderc>"
                "$<TARGET_FILE_DIR:HelloTriangle>"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:spirv-tools::spirv-tools>"
                "$<TARGET_FILE_DIR:HelloTriangle>"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:SDL3::SDL3>"
                "$<TARGET_FILE_DIR:HelloTriangle>"
            COMMENT "Copying HelloTriangle shared runtimes"
        )
    endif()
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
