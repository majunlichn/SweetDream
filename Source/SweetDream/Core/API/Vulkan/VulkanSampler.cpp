#include <SweetDream/Core/API/Vulkan/VulkanSampler.h>
#include <SweetDream/Core/API/Vulkan/VulkanDevice.h>

#include <stdexcept>
#include <utility>

namespace sd
{

VulkanSampler::VulkanSampler(rad::Ref<VulkanDevice> device,
                             const vk::SamplerCreateInfo& createInfo) :
    m_device(std::move(device))
{
    if (!m_device)
    {
        throw std::invalid_argument("VulkanSampler requires a valid VulkanDevice");
    }

    m_handle = m_device->GetHandle().createSampler(createInfo, nullptr, GetDispatcher());
}

VulkanSampler::~VulkanSampler()
{
    if (m_handle)
    {
        m_device->GetHandle().destroySampler(m_handle, nullptr, GetDispatcher());
        m_handle = nullptr;
    }
}

const vk::detail::DispatchLoaderDynamic& VulkanSampler::GetDispatcher() const noexcept
{
    return m_device->GetDispatcher();
}

} // namespace sd
