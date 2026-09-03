#include <SweetDream/Core/API/Vulkan/VulkanRenderPass.h>
#include <SweetDream/Core/API/Vulkan/VulkanDevice.h>

#include <stdexcept>
#include <utility>

namespace sd
{

VulkanRenderPass::VulkanRenderPass(rad::Ref<VulkanDevice> device,
                                   const vk::RenderPassCreateInfo& createInfo) :
    m_device(std::move(device))
{
    if (!m_device)
    {
        throw std::invalid_argument("VulkanRenderPass requires a valid VulkanDevice");
    }

    m_handle =
        m_device->GetHandle().createRenderPass(createInfo, nullptr, GetDispatcher());
}

VulkanRenderPass::~VulkanRenderPass()
{
    if (m_handle)
    {
        m_device->GetHandle().destroyRenderPass(m_handle, nullptr, GetDispatcher());
        m_handle = nullptr;
    }
}

const vk::detail::DispatchLoaderDynamic& VulkanRenderPass::GetDispatcher() const noexcept
{
    return m_device->GetDispatcher();
}

} // namespace sd
