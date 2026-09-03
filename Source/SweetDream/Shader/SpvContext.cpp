#include <SweetDream/Shader/SpvContext.h>

#include <memory>
#include <utility>

namespace sd
{
namespace
{

using DiagnosticPtr = std::unique_ptr<spv_diagnostic_t, decltype(&spvDiagnosticDestroy)>;
using BinaryPtr = std::unique_ptr<spv_binary_t, decltype(&spvBinaryDestroy)>;
using TextPtr = std::unique_ptr<spv_text_t, decltype(&spvTextDestroy)>;

[[nodiscard]] SpvError MakeError(spv_result_t result, const DiagnosticPtr& diagnostic,
                                 std::string fallbackMessage)
{
    SpvError error;
    error.result = result;
    error.message = std::move(fallbackMessage);

    if (diagnostic)
    {
        error.position = diagnostic->position;
        if (diagnostic->error != nullptr && diagnostic->error[0] != '\0')
        {
            error.message = diagnostic->error;
        }
    }

    return error;
}

[[nodiscard]] SpvError MakeContextError()
{
    return SpvError{SPV_ERROR_INVALID_VALUE,
                    "SPIRV-Tools could not create a context for the target environment"};
}

} // namespace

SpvContext::SpvContext(spv_target_env env) :
    m_targetEnv(env),
    m_spvContext(spvContextCreate(m_targetEnv))
{
}

SpvContext::~SpvContext()
{
    spvContextDestroy(m_spvContext);
}

spv_target_env SpvContext::GetTargetEnv() const noexcept
{
    return m_targetEnv;
}

rad::Result<void, SpvError> SpvContext::Validate(const uint32_t* binary,
                                                 size_t wordCount) const
{
    if (m_spvContext == nullptr)
    {
        return rad::Failure(MakeContextError());
    }
    if (wordCount == 0)
    {
        return rad::Failure(
            SpvError{SPV_ERROR_INVALID_BINARY, "SPIR-V binary is empty"});
    }
    if (binary == nullptr)
    {
        return rad::Failure(
            SpvError{SPV_ERROR_INVALID_POINTER, "SPIR-V binary pointer is null"});
    }

    spv_diagnostic diagnostic = nullptr;
    const spv_result_t result =
        spvValidateBinary(m_spvContext, binary, wordCount, &diagnostic);
    DiagnosticPtr ownedDiagnostic(diagnostic, &spvDiagnosticDestroy);
    if (result != SPV_SUCCESS)
    {
        return rad::Failure(
            MakeError(result, ownedDiagnostic, "SPIR-V validation failed"));
    }

    return rad::Success();
}

rad::Result<void, SpvError> SpvContext::Validate(rad::Span<const uint32_t> binary) const
{
    return Validate(binary.data(), binary.size());
}

rad::Result<std::string, SpvError> SpvContext::Disassemble(
    const uint32_t* binary, size_t wordCount, SpvDisassembleOptions options) const
{
    if (m_spvContext == nullptr)
    {
        return rad::Failure(MakeContextError());
    }
    if (wordCount == 0)
    {
        return rad::Failure(
            SpvError{SPV_ERROR_INVALID_BINARY, "SPIR-V binary is empty"});
    }
    if (binary == nullptr)
    {
        return rad::Failure(
            SpvError{SPV_ERROR_INVALID_POINTER, "SPIR-V binary pointer is null"});
    }

    spv_text text = nullptr;
    spv_diagnostic diagnostic = nullptr;
    const spv_result_t result =
        spvBinaryToText(m_spvContext, binary, wordCount, static_cast<uint32_t>(options), &text,
                        &diagnostic);
    TextPtr ownedText(text, &spvTextDestroy);
    DiagnosticPtr ownedDiagnostic(diagnostic, &spvDiagnosticDestroy);
    if (result != SPV_SUCCESS)
    {
        return rad::Failure(
            MakeError(result, ownedDiagnostic, "SPIR-V disassembly failed"));
    }
    if (!ownedText)
    {
        return rad::Failure(
            SpvError{SPV_ERROR_INTERNAL, "SPIRV-Tools returned no disassembly text"});
    }

    return std::string(ownedText->str, ownedText->length);
}

rad::Result<std::string, SpvError> SpvContext::Disassemble(
    rad::Span<const uint32_t> binary, SpvDisassembleOptions options) const
{
    return Disassemble(binary.data(), binary.size(), options);
}

rad::Result<std::vector<uint32_t>, SpvError> SpvContext::Assemble(
    std::string_view assembly, SpvAssembleOptions options) const
{
    if (m_spvContext == nullptr)
    {
        return rad::Failure(MakeContextError());
    }

    spv_binary binary = nullptr;
    spv_diagnostic diagnostic = nullptr;
    const char* text = assembly.empty() ? "" : assembly.data();
    const spv_result_t result = spvTextToBinaryWithOptions(
        m_spvContext, text, assembly.size(), static_cast<uint32_t>(options), &binary,
        &diagnostic);
    BinaryPtr ownedBinary(binary, &spvBinaryDestroy);
    DiagnosticPtr ownedDiagnostic(diagnostic, &spvDiagnosticDestroy);
    if (result != SPV_SUCCESS)
    {
        return rad::Failure(
            MakeError(result, ownedDiagnostic, "SPIR-V assembly failed"));
    }
    if (!ownedBinary)
    {
        return rad::Failure(
            SpvError{SPV_ERROR_INTERNAL, "SPIRV-Tools returned no assembled binary"});
    }

    return std::vector<uint32_t>(ownedBinary->code,
                                 ownedBinary->code + ownedBinary->wordCount);
}

rad::Result<std::string, SpvError> SpvContext::ValidateAndDisassemble(
    rad::Span<const uint32_t> binary, SpvDisassembleOptions options) const
{
    rad::Result<void, SpvError> validation = Validate(binary);
    if (!validation)
    {
        return rad::Failure(std::move(validation).error());
    }

    return Disassemble(binary, options);
}

} // namespace sd
