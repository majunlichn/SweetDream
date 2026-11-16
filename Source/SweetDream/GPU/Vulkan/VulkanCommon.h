#pragma once

#ifndef VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#endif

#include <SweetDream/Core/IO/Logging.h>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_to_string.hpp>

#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vk_mem_alloc.h>

#include <rad/Core/RefCounted.h>
#include <rad/Core/TypeTraits.h>

#include <cassert>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace sd
{

class VulkanVersion
{
public:
    constexpr VulkanVersion() = default;
    constexpr VulkanVersion(uint32_t bits) noexcept :
        m_bits(bits)
    {
    }
    constexpr VulkanVersion(uint32_t major, uint32_t minor, uint32_t patch) noexcept :
        m_bits(VK_MAKE_API_VERSION(0, major, minor, patch))
    {
    }

    constexpr operator uint32_t() const noexcept { return m_bits; }
    [[nodiscard]] std::string ToString() const;

    [[nodiscard]] constexpr uint32_t GetVariant() const noexcept
    {
        return VK_API_VERSION_VARIANT(m_bits);
    }
    [[nodiscard]] constexpr uint32_t GetMajor() const noexcept
    {
        return VK_API_VERSION_MAJOR(m_bits);
    }
    [[nodiscard]] constexpr uint32_t GetMinor() const noexcept
    {
        return VK_API_VERSION_MINOR(m_bits);
    }
    [[nodiscard]] constexpr uint32_t GetPatch() const noexcept
    {
        return VK_API_VERSION_PATCH(m_bits);
    }

    [[nodiscard]] constexpr bool IsSameVariant(const VulkanVersion& other) const noexcept
    {
        return GetVariant() == other.GetVariant();
    }

    [[nodiscard]] constexpr bool IsLowerThan(const VulkanVersion& other) const noexcept
    {
        assert(IsSameVariant(other) && "Cannot compare different Vulkan API variants");
        return m_bits < other.m_bits;
    }

    [[nodiscard]] constexpr bool IsLowerEqualThan(const VulkanVersion& other) const noexcept
    {
        assert(IsSameVariant(other) && "Cannot compare different Vulkan API variants");
        return m_bits <= other.m_bits;
    }

    [[nodiscard]] constexpr bool IsGreaterThan(const VulkanVersion& other) const noexcept
    {
        assert(IsSameVariant(other) && "Cannot compare different Vulkan API variants");
        return m_bits > other.m_bits;
    }

    [[nodiscard]] constexpr bool IsGreaterEqualThan(const VulkanVersion& other) const noexcept
    {
        assert(IsSameVariant(other) && "Cannot compare different Vulkan API variants");
        return m_bits >= other.m_bits;
    }

    constexpr bool operator==(const VulkanVersion& other) const noexcept = default;
    constexpr std::strong_ordering operator<=>(const VulkanVersion& other) const noexcept
    {
        assert(IsSameVariant(other) && "Cannot compare different Vulkan API variants");
        return m_bits <=> other.m_bits;
    }

private:
    uint32_t m_bits = 0;
}; // class VulkanVersion

inline constexpr VulkanVersion TargetVulkanApiVersion = VK_API_VERSION_1_4;

[[nodiscard]] bool Contains(const std::vector<vk::LayerProperties>& properties,
                            std::string_view name);
[[nodiscard]] bool Contains(const std::vector<vk::ExtensionProperties>& properties,
                            std::string_view name);

// vk::Flags helpers (HasAllBits / HasAnyBits / HasNoBits).
template <typename BitType>
[[nodiscard]] constexpr bool HasAllBits(vk::Flags<BitType> flags,
                                        vk::Flags<BitType> bits) noexcept
{
    return (flags & bits) == bits;
}

template <typename BitType>
[[nodiscard]] constexpr bool HasAllBits(vk::Flags<BitType> flags, BitType bits) noexcept
{
    return HasAllBits(flags, vk::Flags<BitType>{bits});
}

template <typename BitType>
[[nodiscard]] constexpr bool HasAnyBits(vk::Flags<BitType> flags,
                                        vk::Flags<BitType> bits) noexcept
{
    return static_cast<bool>(flags & bits);
}

template <typename BitType>
[[nodiscard]] constexpr bool HasAnyBits(vk::Flags<BitType> flags, BitType bits) noexcept
{
    return HasAnyBits(flags, vk::Flags<BitType>{bits});
}

template <typename BitType>
[[nodiscard]] constexpr bool HasNoBits(vk::Flags<BitType> flags,
                                       vk::Flags<BitType> bits) noexcept
{
    return (flags & bits) == vk::Flags<BitType>{};
}

template <typename BitType>
[[nodiscard]] constexpr bool HasNoBits(vk::Flags<BitType> flags, BitType bits) noexcept
{
    return HasNoBits(flags, vk::Flags<BitType>{bits});
}

// Throws only for negative Vulkan failure codes. Success and status codes pass through.
inline vk::Result CheckVulkanResult(
    vk::Result result, const char* expression,
    std::source_location sourceLocation = std::source_location::current())
{
    if (rad::UnderlyingCast(result) < 0)
    {
        SD_LOG(err, "{} failed: {} (at {}:{} in {})", expression, vk::to_string(result),
               sourceLocation.file_name(), sourceLocation.line(), sourceLocation.function_name());
        throw vk::SystemError(vk::make_error_code(result), vk::to_string(result));
    }
    return result;
}

// Overload for Vulkan C APIs and VMA.
inline vk::Result CheckVulkanResult(
    VkResult result, const char* expression,
    std::source_location sourceLocation = std::source_location::current())
{
    return CheckVulkanResult(static_cast<vk::Result>(result), expression, sourceLocation);
}

template <typename T>
concept VulkanStructureConcept = requires(T structure) {
    requires std::is_standard_layout_v<T>;
    requires offsetof(T, sType) == 0;
    requires offsetof(T, pNext) == offsetof(vk::BaseInStructure, pNext);
    requires std::is_same_v<decltype(structure.sType), VkStructureType> ||
                 std::is_same_v<decltype(structure.sType), vk::StructureType>;
    { structure.pNext } -> std::convertible_to<const void*>;
};

template <typename T>
concept VulkanBaseOutStructureConcept = VulkanStructureConcept<T> && requires(T structure) {
    { structure.pNext } -> std::convertible_to<void*>;
};

// Lifetime is owned by the caller; every linked structure must outlive the Vulkan call.
template <VulkanBaseOutStructureConcept Head>
class VulkanStructureChain
{
public:
    using Iterator = vk::BaseOutStructure*;
    using ConstIterator = const vk::BaseOutStructure*;

    explicit VulkanStructureChain(Head& head) :
        m_head(head)
    {
        Reset();
    }

    operator Head&() noexcept { return m_head; }
    operator const Head&() const noexcept { return m_head; }
    Head* operator&() noexcept { return &m_head; }

    void Reset() noexcept
    {
        m_head.pNext = nullptr;
        m_tail = reinterpret_cast<Iterator>(&m_head);
    }

    template <VulkanBaseOutStructureConcept Node>
    void Link(Node& next) noexcept
    {
        next.pNext = nullptr;
        auto* node = reinterpret_cast<Iterator>(&next);
        m_tail->pNext = node;
        m_tail = node;
    }

    [[nodiscard]] std::string ToString() const
    {
        std::string string;
        ConstIterator current = reinterpret_cast<ConstIterator>(&m_head);
        while (current != nullptr)
        {
            string += vk::to_string(current->sType) + "->";
            current = reinterpret_cast<ConstIterator>(current->pNext);
        }
        string += "Null";
        return string;
    }

private:
    Head& m_head;
    Iterator m_tail = nullptr;
}; // class VulkanStructureChain

} // namespace sd

#define SD_CHECK_VKRESULT(Expression) ::sd::CheckVulkanResult((Expression), #Expression)
