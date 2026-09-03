#include <SweetDream/Core/API/Vulkan/VulkanFramebuffer.h>
#include <SweetDream/Core/API/Vulkan/VulkanDevice.h>
#include <SweetDream/Core/API/Vulkan/VulkanImage.h>
#include <SweetDream/Core/API/Vulkan/VulkanRenderPass.h>

#include <limits>
#include <stdexcept>
#include <utility>

namespace sd
{

VulkanFramebuffer::VulkanFramebuffer(
    rad::Ref<VulkanDevice> device, const VulkanFramebufferCreateInfo& createInfo) :
    m_device(std::move(device)),
    m_attachments(createInfo.m_attachments),
    m_width(createInfo.m_width),
    m_height(createInfo.m_height),
    m_layers(createInfo.m_layers)
{
    if (!m_device)
    {
        throw std::invalid_argument("VulkanFramebuffer requires a valid VulkanDevice");
    }
    if (!createInfo.m_renderPass)
    {
        throw std::invalid_argument("VulkanFramebuffer requires a render pass");
    }
    if (createInfo.m_renderPass->GetDevice() != m_device.get())
    {
        throw std::invalid_argument(
            "VulkanFramebuffer render pass belongs to a different VulkanDevice");
    }
    if (m_width == 0 || m_height == 0 || m_layers == 0)
    {
        throw std::invalid_argument(
            "VulkanFramebuffer dimensions and layer count must be greater than zero");
    }
    if (m_attachments.size() > std::numeric_limits<uint32_t>::max())
    {
        throw std::length_error("VulkanFramebuffer has too many attachments");
    }

    std::vector<vk::ImageView> attachments;
    attachments.reserve(m_attachments.size());
    for (const rad::Ref<VulkanImageView>& attachment : m_attachments)
    {
        if (!attachment)
        {
            throw std::invalid_argument(
                "VulkanFramebuffer attachments must contain valid image views");
        }
        if (attachment->GetDevice() != m_device.get())
        {
            throw std::invalid_argument(
                "VulkanFramebuffer attachment belongs to a different VulkanDevice");
        }
        attachments.push_back(attachment->GetHandle());
    }

    vk::FramebufferCreateInfo vkCreateInfo;
    vkCreateInfo.renderPass = createInfo.m_renderPass->GetHandle();
    vkCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    vkCreateInfo.pAttachments = attachments.data();
    vkCreateInfo.width = m_width;
    vkCreateInfo.height = m_height;
    vkCreateInfo.layers = m_layers;

    m_handle =
        m_device->GetHandle().createFramebuffer(vkCreateInfo, nullptr, GetDispatcher());
}

VulkanFramebuffer::~VulkanFramebuffer()
{
    if (m_handle)
    {
        m_device->GetHandle().destroyFramebuffer(m_handle, nullptr, GetDispatcher());
        m_handle = nullptr;
    }
}

const vk::detail::DispatchLoaderDynamic& VulkanFramebuffer::GetDispatcher() const noexcept
{
    return m_device->GetDispatcher();
}

} // namespace sd
