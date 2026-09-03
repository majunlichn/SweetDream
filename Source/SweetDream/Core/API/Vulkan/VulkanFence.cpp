#include <SweetDream/Core/API/Vulkan/VulkanFence.h>
#include <SweetDream/Core/API/Vulkan/VulkanDevice.h>

#include <stdexcept>
#include <utility>

namespace sd
{

VulkanFence::VulkanFence(rad::Ref<VulkanDevice> device,
                         const vk::FenceCreateInfo& createInfo) :
    m_device(std::move(device))
{
    if (!m_device)
    {
        throw std::invalid_argument("VulkanFence requires a valid VulkanDevice");
    }

    m_handle = m_device->GetHandle().createFence(createInfo, nullptr, GetDispatcher());
}

VulkanFence::~VulkanFence()
{
    if (m_handle)
    {
        m_device->GetHandle().destroyFence(m_handle, nullptr, GetDispatcher());
        m_handle = nullptr;
    }
}

const vk::detail::DispatchLoaderDynamic& VulkanFence::GetDispatcher() const noexcept
{
    return m_device->GetDispatcher();
}

vk::Result VulkanFence::Wait(uint64_t timeout)
{
    return SD_CHECK_VKRESULT(
        m_device->GetHandle().waitForFences(m_handle, true, timeout, GetDispatcher()));
}

void VulkanFence::Reset()
{
    m_device->GetHandle().resetFences(m_handle, GetDispatcher());
}

} // namespace sd
