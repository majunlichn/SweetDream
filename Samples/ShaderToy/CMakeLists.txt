add_executable(ShaderToy
    ShaderToy.cpp
    ShaderToyConfig.h
    ShaderToyConfig.cpp
    ShaderToyRenderer.h
    ShaderToyRenderer.cpp
    ShaderToyTexture.h
    ShaderToyTexture.cpp
    download_resources.py
    ShaderToy.vert
    ShaderToy.frag
)
set_source_files_properties(ShaderToy.vert ShaderToy.frag
    PROPERTIES HEADER_FILE_ONLY TRUE)
target_link_libraries(ShaderToy PRIVATE SweetDream::SweetDream)
set_target_properties(ShaderToy PROPERTIES
    FOLDER Samples
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)

add_custom_target(ShaderToyAssets
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ShaderToy"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/download_resources.py"
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ShaderToy"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/ShaderToy.vert"
        "${CMAKE_CURRENT_SOURCE_DIR}/ShaderToy.frag"
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ShaderToy"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders"
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ShaderToy/Shaders"
    COMMENT "Copying ShaderToy shaders and JSON configurations"
)
add_dependencies(ShaderToy ShaderToyAssets)

if(WIN32)
    add_custom_command(TARGET ShaderToy POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:shaderc::shaderc>"
            "$<TARGET_FILE_DIR:ShaderToy>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:spirv-tools::spirv-tools>"
            "$<TARGET_FILE_DIR:ShaderToy>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:SDL3::SDL3>"
            "$<TARGET_FILE_DIR:ShaderToy>"
    )
endif()
