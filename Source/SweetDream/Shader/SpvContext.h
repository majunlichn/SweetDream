#pragma once

#include <rad/Core/RefCounted.h>
#include <rad/Core/Result.h>
#include <rad/Core/Span.h>

#include <spirv-tools/libspirv.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace sd
{

struct SpvError
{
    spv_result_t result = SPV_ERROR_INTERNAL;
    std::string message;
    spv_position_t position = {};
};

enum class SpvDisassembleOptions : uint32_t
{
    None = SPV_BINARY_TO_TEXT_OPTION_NONE,
    Indent = SPV_BINARY_TO_TEXT_OPTION_INDENT,
    ShowByteOffset = SPV_BINARY_TO_TEXT_OPTION_SHOW_BYTE_OFFSET,
    NoHeader = SPV_BINARY_TO_TEXT_OPTION_NO_HEADER,
    FriendlyNames = SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES,
    Comments = SPV_BIT(7),              // SPV_BINARY_TO_TEXT_OPTION_COMMENT (v2022.x+)
    NestedIndent = SPV_BIT(8),          // SPV_BINARY_TO_TEXT_OPTION_NESTED_INDENT (v2024.3+)
    ReorderBlocks = SPV_BIT(9),         // SPV_BINARY_TO_TEXT_OPTION_REORDER_BLOCKS (v2024.3+)
    HandleUnknownOpcodes = SPV_BIT(10), // SPV_BINARY_TO_TEXT_OPTION_HANDLE_UNKNOWN_OPCODES (v2025.4+)

    Default = SPV_BINARY_TO_TEXT_OPTION_NONE,
};

enum class SpvAssembleOptions : uint32_t
{
    None = SPV_TEXT_TO_BINARY_OPTION_NONE,
    PreserveNumericIds = SPV_TEXT_TO_BINARY_OPTION_PRESERVE_NUMERIC_IDS,

    Default = SPV_TEXT_TO_BINARY_OPTION_NONE,
};

[[nodiscard]] constexpr SpvDisassembleOptions operator|(SpvDisassembleOptions lhs,
                                                        SpvDisassembleOptions rhs) noexcept
{
    return static_cast<SpvDisassembleOptions>(static_cast<uint32_t>(lhs) |
                                              static_cast<uint32_t>(rhs));
}

[[nodiscard]] constexpr SpvAssembleOptions operator|(SpvAssembleOptions lhs,
                                                     SpvAssembleOptions rhs) noexcept
{
    return static_cast<SpvAssembleOptions>(static_cast<uint32_t>(lhs) |
                                           static_cast<uint32_t>(rhs));
}

class SpvContext : public rad::RefCounted<SpvContext>
{
public:
    explicit SpvContext(spv_target_env env = SPV_ENV_VULKAN_1_4);
    ~SpvContext();

    SpvContext(const SpvContext&) = delete;
    SpvContext& operator=(const SpvContext&) = delete;

    [[nodiscard]] spv_target_env GetTargetEnv() const noexcept;

    [[nodiscard]] rad::Result<void, SpvError> Validate(const uint32_t* binary,
                                                       size_t wordCount) const;
    [[nodiscard]] rad::Result<void, SpvError> Validate(
        rad::Span<const uint32_t> binary) const;

    [[nodiscard]] rad::Result<std::string, SpvError> Disassemble(
        const uint32_t* binary, size_t wordCount,
        SpvDisassembleOptions options = SpvDisassembleOptions::Default) const;
    [[nodiscard]] rad::Result<std::string, SpvError> Disassemble(
        rad::Span<const uint32_t> binary,
        SpvDisassembleOptions options = SpvDisassembleOptions::Default) const;

    [[nodiscard]] rad::Result<std::vector<uint32_t>, SpvError> Assemble(
        std::string_view assembly,
        SpvAssembleOptions options = SpvAssembleOptions::Default) const;

    [[nodiscard]] rad::Result<std::string, SpvError> ValidateAndDisassemble(
        rad::Span<const uint32_t> binary,
        SpvDisassembleOptions options = SpvDisassembleOptions::Default) const;

private:
    const spv_target_env m_targetEnv;
    spv_context m_spvContext = nullptr;
}; // class SpvContext

} // namespace sd
