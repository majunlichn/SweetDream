#pragma once

#include <rad/Core/RefCounted.h>
#include <rad/Core/Result.h>
#include <rad/Core/Span.h>
#include <rad/System/FileSystem.h>

#include <shaderc/env.h>
#include <vulkan/vulkan.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace shaderc
{
class Compiler;
}

namespace sd
{

struct GLSLMacro
{
    GLSLMacro() = default;

    GLSLMacro(std::string_view name) :
        m_name(name)
    {
    }

    GLSLMacro(std::string_view name, std::string_view value) :
        m_name(name),
        m_value(value)
    {
    }

    std::string m_name;
    std::string m_value;
}; // struct GLSLMacro

enum class GLSLCompileOptLevel
{
    Zero,
    Size,
    Performance,
};

struct GLSLCompileError
{
    std::string message;
};

class GLSLCompiler : public rad::RefCounted<GLSLCompiler>
{
public:
    GLSLCompiler();
    ~GLSLCompiler();

    GLSLCompiler(const GLSLCompiler&) = delete;
    GLSLCompiler& operator=(const GLSLCompiler&) = delete;

    void SetTargetVulkanVersion(shaderc_env_version version);
    void SetTargetSpirvVersion(shaderc_spirv_version version);

    void SetIncludeDirs(const std::vector<rad::FilePath>& includeDirs);
    void AddIncludeDir(rad::FilePath includeDir);

    [[nodiscard]] rad::Result<std::string, GLSLCompileError> Preprocess(
        vk::ShaderStageFlagBits stage, const std::string& fileName, const std::string& source,
        rad::Span<const GLSLMacro> macros = {});

    [[nodiscard]] rad::Result<std::vector<uint32_t>, GLSLCompileError> CompileToSpv(
        vk::ShaderStageFlagBits stage, const std::string& fileName, const std::string& source,
        const std::string& entryPoint = "main", rad::Span<const GLSLMacro> macros = {},
        GLSLCompileOptLevel opt = GLSLCompileOptLevel::Zero);

    [[nodiscard]] rad::Result<std::vector<uint32_t>, GLSLCompileError> CompileFileToSpv(
        vk::ShaderStageFlagBits stage, const rad::FilePath& filePath,
        const std::string& entryPoint = "main", rad::Span<const GLSLMacro> macros = {},
        GLSLCompileOptLevel opt = GLSLCompileOptLevel::Zero);

private:
    std::unique_ptr<shaderc::Compiler> m_compiler;
    std::vector<rad::FilePath> m_includeDirs;
    shaderc_env_version m_targetVulkanVersion = shaderc_env_version_vulkan_1_4;
    shaderc_spirv_version m_targetSpirvVersion = shaderc_spirv_version_1_6;
}; // class GLSLCompiler

} // namespace sd
