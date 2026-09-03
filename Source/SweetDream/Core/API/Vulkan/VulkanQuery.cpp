#include <SweetDream/Core/API/Vulkan/VulkanQuery.h>
#include <SweetDream/Core/API/Vulkan/VulkanDevice.h>

#include <stdexcept>
#include <utility>

namespace sd
{

VulkanQueryPool::VulkanQueryPool(rad::Ref<VulkanDevice> device,
                                 const vk::QueryPoolCreateInfo& createInfo) :
    m_device(std::move(device)),
    m_flags(createInfo.flags),
    m_queryType(createInfo.queryType),
    m_queryCount(createInfo.queryCount),
    m_pipelineStatistics(createInfo.pipelineStatistics)
{
    if (!m_device)
    {
        throw std::invalid_argument("VulkanQueryPool requires a valid VulkanDevice");
    }
    if (m_queryCount == 0)
    {
        throw std::invalid_argument("VulkanQueryPool query count must be greater than zero");
    }

    m_handle =
        m_device->GetHandle().createQueryPool(createInfo, nullptr, GetDispatcher());
}

VulkanQueryPool::~VulkanQueryPool()
{
    if (m_handle)
    {
        m_device->GetHandle().destroyQueryPool(m_handle, nullptr, GetDispatcher());
        m_handle = nullptr;
    }
}

const vk::detail::DispatchLoaderDynamic& VulkanQueryPool::GetDispatcher() const noexcept
{
    return m_device->GetDispatcher();
}

void VulkanQueryPool::Reset(uint32_t firstQuery, uint32_t queryCount)
{
    if (firstQuery >= m_queryCount || queryCount == 0 ||
        queryCount > m_queryCount - firstQuery)
    {
        throw std::out_of_range("VulkanQueryPool reset range is invalid");
    }

    m_device->GetHandle().resetQueryPool(m_handle, firstQuery, queryCount,
                                         GetDispatcher());
}

vk::Result VulkanQueryPool::GetResults(
    uint32_t firstQuery, uint32_t queryCount, size_t dataSize, void* data,
    vk::DeviceSize stride, vk::QueryResultFlags flags) const
{
    if (firstQuery >= m_queryCount || queryCount == 0 ||
        queryCount > m_queryCount - firstQuery)
    {
        throw std::out_of_range("VulkanQueryPool result range is invalid");
    }

    return SD_CHECK_VKRESULT(m_device->GetHandle().getQueryPoolResults(
        m_handle, firstQuery, queryCount, dataSize, data, stride, flags,
        GetDispatcher()));
}

} // namespace sd
