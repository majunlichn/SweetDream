#include <SweetDream/Core/API/Vulkan/VulkanSemaphore.h>
#include <SweetDream/Core/API/Vulkan/VulkanDevice.h>

#include <stdexcept>
#include <utility>

namespace sd
{

VulkanSemaphore::VulkanSemaphore(rad::Ref<VulkanDevice> device,
                                 const vk::SemaphoreCreateInfo& createInfo) :
    m_device(std::move(device))
{
    if (!m_device)
    {
        throw std::invalid_argument("VulkanSemaphore requires a valid VulkanDevice");
    }

    m_handle =
        m_device->GetHandle().createSemaphore(createInfo, nullptr, GetDispatcher());
}

VulkanSemaphore::~VulkanSemaphore()
{
    if (m_handle)
    {
        m_device->GetHandle().destroySemaphore(m_handle, nullptr, GetDispatcher());
        m_handle = nullptr;
    }
}

const vk::detail::DispatchLoaderDynamic& VulkanSemaphore::GetDispatcher() const noexcept
{
    return m_device->GetDispatcher();
}

} // namespace sd
