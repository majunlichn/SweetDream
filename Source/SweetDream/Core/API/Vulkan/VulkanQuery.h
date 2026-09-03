#pragma once

#include <SweetDream/Core/API/Vulkan/VulkanCommon.h>

#include <span>

namespace sd
{

class VulkanDevice;

class VulkanQueryPool : public rad::RefCounted<VulkanQueryPool>
{
public:
    VulkanQueryPool(rad::Ref<VulkanDevice> device,
                    const vk::QueryPoolCreateInfo& createInfo);
    ~VulkanQueryPool();

    VulkanQueryPool(const VulkanQueryPool&) = delete;
    VulkanQueryPool& operator=(const VulkanQueryPool&) = delete;

    [[nodiscard]] VulkanDevice* GetDevice() const noexcept { return m_device.get(); }
    [[nodiscard]] const vk::detail::DispatchLoaderDynamic& GetDispatcher() const noexcept;
    [[nodiscard]] const vk::QueryPool& GetHandle() const noexcept { return m_handle; }

    [[nodiscard]] vk::QueryPoolCreateFlags GetFlags() const noexcept { return m_flags; }
    [[nodiscard]] vk::QueryType GetQueryType() const noexcept { return m_queryType; }
    [[nodiscard]] uint32_t GetQueryCount() const noexcept { return m_queryCount; }
    [[nodiscard]] vk::QueryPipelineStatisticFlags GetPipelineStatistics() const noexcept
    {
        return m_pipelineStatistics;
    }

    void Reset(uint32_t firstQuery, uint32_t queryCount);
    void Reset() { Reset(0, m_queryCount); }

    template <typename T>
    [[nodiscard]] vk::Result GetResults(
        uint32_t firstQuery, uint32_t queryCount, std::span<T> data,
        vk::DeviceSize stride = sizeof(T), vk::QueryResultFlags flags = {}) const
    {
        return GetResults(firstQuery, queryCount, data.size_bytes(), data.data(), stride,
                          flags);
    }

private:
    [[nodiscard]] vk::Result GetResults(
        uint32_t firstQuery, uint32_t queryCount, size_t dataSize, void* data,
        vk::DeviceSize stride, vk::QueryResultFlags flags) const;

    rad::Ref<VulkanDevice> m_device;
    vk::QueryPool m_handle = nullptr;

    vk::QueryPoolCreateFlags m_flags = {};
    vk::QueryType m_queryType = vk::QueryType::eOcclusion;
    uint32_t m_queryCount = {};
    vk::QueryPipelineStatisticFlags m_pipelineStatistics = {};
}; // class VulkanQueryPool

} // namespace sd
