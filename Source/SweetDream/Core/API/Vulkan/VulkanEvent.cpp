#include <SweetDream/Core/API/Vulkan/VulkanEvent.h>
#include <SweetDream/Core/API/Vulkan/VulkanDevice.h>

#include <stdexcept>
#include <utility>

namespace sd
{

VulkanEvent::VulkanEvent(rad::Ref<VulkanDevice> device,
                         const vk::EventCreateInfo& createInfo) :
    m_device(std::move(device))
{
    if (!m_device)
    {
        throw std::invalid_argument("VulkanEvent requires a valid VulkanDevice");
    }

    m_handle = m_device->GetHandle().createEvent(createInfo, nullptr, GetDispatcher());
}

VulkanEvent::~VulkanEvent()
{
    if (m_handle)
    {
        m_device->GetHandle().destroyEvent(m_handle, nullptr, GetDispatcher());
        m_handle = nullptr;
    }
}

const vk::detail::DispatchLoaderDynamic& VulkanEvent::GetDispatcher() const noexcept
{
    return m_device->GetDispatcher();
}

vk::Result VulkanEvent::GetStatus() const
{
    return SD_CHECK_VKRESULT(
        m_device->GetHandle().getEventStatus(m_handle, GetDispatcher()));
}

void VulkanEvent::Set()
{
    m_device->GetHandle().setEvent(m_handle, GetDispatcher());
}

void VulkanEvent::Reset()
{
    m_device->GetHandle().resetEvent(m_handle, GetDispatcher());
}

} // namespace sd
