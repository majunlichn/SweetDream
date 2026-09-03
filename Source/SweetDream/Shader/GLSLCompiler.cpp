#include <SweetDream/Shader/GLSLCompiler.h>

#include <rad/IO/File.h>

#include <shaderc/shaderc.hpp>

#include <exception>
#include <filesystem>
#include <optional>
#include <utility>

namespace sd
{
namespace
{

// Include search follows shaderc_util::FileFinder: relative includes first use the requesting
// file's directory, then all includes use the configured search paths.
// https://github.com/google/shaderc/blob/main/libshaderc_util/include/libshaderc_util/file_finder.h
// https://github.com/google/shaderc/blob/main/libshaderc_util/src/file_finder.cc
class FileIncluder final : public shaderc::CompileOptions::IncluderInterface
{
public:
    explicit FileIncluder(const std::vector<rad::FilePath>& includeDirs)
    {
        m_searchPaths.reserve(includeDirs.size());
        for (const rad::FilePath& includeDir : includeDirs)
        {
            m_searchPaths.push_back(includeDir);
        }
    }

    shaderc_include_result* GetInclude(const char* requestedSource, shaderc_include_type type,
                                       const char* requestingSource, size_t) noexcept override
    {
        try
        {
            const std::string requestedName = requestedSource != nullptr ? requestedSource : "";
            if (requestedName.empty())
            {
                return MakeErrorResult("Cannot include an empty file name");
            }

            const rad::FilePath requestedPath = rad::MakeFilePath(requestedName);
            if (requestedPath.is_absolute())
            {
                if (shaderc_include_result* result = TryLoadInclude(requestedPath))
                {
                    return result;
                }
            }
            else if (type == shaderc_include_type_relative)
            {
                const std::string requester = requestingSource != nullptr ? requestingSource : "";
                const rad::FilePath requesterPath = rad::MakeFilePath(requester);
                if (shaderc_include_result* result =
                        TryLoadInclude(requesterPath.parent_path() / requestedPath))
                {
                    return result;
                }
            }

            for (const rad::FilePath& searchPath : m_searchPaths)
            {
                if (shaderc_include_result* result =
                        TryLoadInclude(searchPath / requestedPath))
                {
                    return result;
                }
            }

            return MakeErrorResult("Cannot find or read include file: " + requestedName);
        }
        catch (const std::exception& error)
        {
            return MakeExceptionErrorResult(error.what());
        }
        catch (...)
        {
            return MakeExceptionErrorResult("unknown error");
        }
    }

    void ReleaseInclude(shaderc_include_result* data) noexcept override
    {
        if (data != nullptr)
        {
            delete static_cast<OwnedIncludeResult*>(data->user_data);
        }
    }

private:
    struct OwnedIncludeResult
    {
        shaderc_include_result result = {};
        std::string sourceName;
        std::string content;
    };

    [[nodiscard]] static shaderc_include_result* MakeResult(std::string sourceName,
                                                            std::string content)
    {
        auto* owned = new OwnedIncludeResult;
        owned->sourceName = std::move(sourceName);
        owned->content = std::move(content);
        owned->result.source_name = owned->sourceName.c_str();
        owned->result.source_name_length = owned->sourceName.size();
        owned->result.content = owned->content.c_str();
        owned->result.content_length = owned->content.size();
        owned->result.user_data = owned;
        return &owned->result;
    }

    [[nodiscard]] static shaderc_include_result* MakeErrorResult(std::string message)
    {
        return MakeResult({}, std::move(message));
    }

    [[nodiscard]] static shaderc_include_result* MakeExceptionErrorResult(
        const char* detail) noexcept
    {
        try
        {
            return MakeErrorResult("Failed to resolve shader include: " + std::string(detail));
        }
        catch (...)
        {
            static constexpr char message[] = "Failed to resolve shader include";
            static shaderc_include_result result = {
                "", 0, message, sizeof(message) - 1, nullptr};
            return &result;
        }
    }

    [[nodiscard]] static rad::FilePath MakeResolvedPath(const rad::FilePath& path)
    {
        std::error_code error;
        rad::FilePath resolved = std::filesystem::weakly_canonical(path, error);
        if (!error)
        {
            return resolved;
        }

        error.clear();
        resolved = std::filesystem::absolute(path, error);
        return error ? path.lexically_normal() : resolved.lexically_normal();
    }

    [[nodiscard]] static shaderc_include_result* TryLoadInclude(const rad::FilePath& path)
    {
        std::optional<std::string> content = rad::File::ReadAllText(path);
        if (!content)
        {
            return nullptr;
        }

        return MakeResult(rad::ToUtf8(MakeResolvedPath(path)), std::move(*content));
    }

    std::vector<rad::FilePath> m_searchPaths;
}; // class FileIncluder

[[nodiscard]] std::optional<shaderc_shader_kind> ToShaderKind(vk::ShaderStageFlagBits stage)
{
    switch (stage)
    {
    case vk::ShaderStageFlagBits::eVertex:
        return shaderc_vertex_shader;
    case vk::ShaderStageFlagBits::eTessellationControl:
        return shaderc_tess_control_shader;
    case vk::ShaderStageFlagBits::eTessellationEvaluation:
        return shaderc_tess_evaluation_shader;
    case vk::ShaderStageFlagBits::eGeometry:
        return shaderc_geometry_shader;
    case vk::ShaderStageFlagBits::eFragment:
        return shaderc_fragment_shader;
    case vk::ShaderStageFlagBits::eCompute:
        return shaderc_compute_shader;
    case vk::ShaderStageFlagBits::eRaygenKHR:
        return shaderc_raygen_shader;
    case vk::ShaderStageFlagBits::eAnyHitKHR:
        return shaderc_anyhit_shader;
    case vk::ShaderStageFlagBits::eClosestHitKHR:
        return shaderc_closesthit_shader;
    case vk::ShaderStageFlagBits::eMissKHR:
        return shaderc_miss_shader;
    case vk::ShaderStageFlagBits::eIntersectionKHR:
        return shaderc_intersection_shader;
    case vk::ShaderStageFlagBits::eCallableKHR:
        return shaderc_callable_shader;
    case vk::ShaderStageFlagBits::eTaskEXT:
        return shaderc_task_shader;
    case vk::ShaderStageFlagBits::eMeshEXT:
        return shaderc_mesh_shader;
    default:
        return std::nullopt;
    }
}

[[nodiscard]] shaderc_optimization_level ToShadercOptimization(GLSLCompileOptLevel level)
{
    switch (level)
    {
    case GLSLCompileOptLevel::Zero:
        return shaderc_optimization_level_zero;
    case GLSLCompileOptLevel::Size:
        return shaderc_optimization_level_size;
    case GLSLCompileOptLevel::Performance:
        return shaderc_optimization_level_performance;
    }

    return shaderc_optimization_level_zero;
}

[[nodiscard]] shaderc::CompileOptions MakeCompileOptions(
    shaderc_env_version targetVulkanVersion, shaderc_spirv_version targetSpirvVersion,
    const std::vector<rad::FilePath>& includeDirs, rad::Span<const GLSLMacro> macros,
    GLSLCompileOptLevel opt)
{
    shaderc::CompileOptions options;
    options.SetTargetEnvironment(shaderc_target_env_vulkan, targetVulkanVersion);
    options.SetTargetSpirv(targetSpirvVersion);
    options.SetOptimizationLevel(ToShadercOptimization(opt));
    options.SetIncluder(std::make_unique<FileIncluder>(includeDirs));

    for (const GLSLMacro& macro : macros)
    {
        options.AddMacroDefinition(macro.m_name, macro.m_value);
    }

    return options;
}

} // namespace

GLSLCompiler::GLSLCompiler() :
    m_compiler(std::make_unique<shaderc::Compiler>())
{
}

GLSLCompiler::~GLSLCompiler() = default;

void GLSLCompiler::SetTargetVulkanVersion(shaderc_env_version version)
{
    m_targetVulkanVersion = version;
}

void GLSLCompiler::SetTargetSpirvVersion(shaderc_spirv_version version)
{
    m_targetSpirvVersion = version;
}

void GLSLCompiler::SetIncludeDirs(const std::vector<rad::FilePath>& includeDirs)
{
    m_includeDirs = includeDirs;
}

void GLSLCompiler::AddIncludeDir(rad::FilePath includeDir)
{
    m_includeDirs.push_back(std::move(includeDir));
}

rad::Result<std::string, GLSLCompileError> GLSLCompiler::Preprocess(
    vk::ShaderStageFlagBits stage, const std::string& fileName, const std::string& source,
    rad::Span<const GLSLMacro> macros)
{
    std::optional<shaderc_shader_kind> shaderKind = ToShaderKind(stage);
    if (!shaderKind)
    {
        return rad::Failure(
            GLSLCompileError{"Unsupported Vulkan shader stage: " + vk::to_string(stage)});
    }
    if (!m_compiler || !m_compiler->IsValid())
    {
        return rad::Failure(GLSLCompileError{"Failed to initialize shaderc compiler"});
    }

    shaderc::CompileOptions options =
        MakeCompileOptions(m_targetVulkanVersion, m_targetSpirvVersion, m_includeDirs, macros,
                           GLSLCompileOptLevel::Zero);
    shaderc::PreprocessedSourceCompilationResult result =
        m_compiler->PreprocessGlsl(source, *shaderKind, fileName.c_str(), options);
    if (result.GetCompilationStatus() != shaderc_compilation_status_success)
    {
        return rad::Failure(GLSLCompileError{result.GetErrorMessage()});
    }

    return std::string(result.cbegin(), result.cend());
}

rad::Result<std::vector<uint32_t>, GLSLCompileError> GLSLCompiler::CompileToSpv(
    vk::ShaderStageFlagBits stage, const std::string& fileName, const std::string& source,
    const std::string& entryPoint, rad::Span<const GLSLMacro> macros, GLSLCompileOptLevel opt)
{
    std::optional<shaderc_shader_kind> shaderKind = ToShaderKind(stage);
    if (!shaderKind)
    {
        return rad::Failure(
            GLSLCompileError{"Unsupported Vulkan shader stage: " + vk::to_string(stage)});
    }
    if (!m_compiler || !m_compiler->IsValid())
    {
        return rad::Failure(GLSLCompileError{"Failed to initialize shaderc compiler"});
    }

    shaderc::CompileOptions options = MakeCompileOptions(
        m_targetVulkanVersion, m_targetSpirvVersion, m_includeDirs, macros, opt);
    shaderc::SpvCompilationResult result = m_compiler->CompileGlslToSpv(
        source, *shaderKind, fileName.c_str(), entryPoint.c_str(), options);
    if (result.GetCompilationStatus() != shaderc_compilation_status_success)
    {
        return rad::Failure(GLSLCompileError{result.GetErrorMessage()});
    }

    return std::vector<uint32_t>(result.cbegin(), result.cend());
}

rad::Result<std::vector<uint32_t>, GLSLCompileError> GLSLCompiler::CompileFileToSpv(
    vk::ShaderStageFlagBits stage, const rad::FilePath& filePath, const std::string& entryPoint,
    rad::Span<const GLSLMacro> macros, GLSLCompileOptLevel opt)
{
    std::optional<std::string> source = rad::File::ReadAllText(filePath);
    const std::string utf8FilePath = rad::ToUtf8(filePath);
    if (!source)
    {
        return rad::Failure(
            GLSLCompileError{"Cannot read GLSL source file: " + utf8FilePath});
    }

    return CompileToSpv(stage, utf8FilePath, *source, entryPoint, macros, opt);
}

} // namespace sd
