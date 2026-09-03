#pragma once

#include <SweetDream/Core/API/Vulkan/VulkanCommon.h>

namespace sd
{

class VulkanDevice;
class VulkanImageView;
class VulkanRenderPass;

struct VulkanFramebufferCreateInfo
{
    rad::Ref<VulkanRenderPass> m_renderPass;
    std::vector<rad::Ref<VulkanImageView>> m_attachments;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_layers = 1;
}; // struct VulkanFramebufferCreateInfo

class VulkanFramebuffer : public rad::RefCounted<VulkanFramebuffer>
{
public:
    VulkanFramebuffer(rad::Ref<VulkanDevice> device,
                      const VulkanFramebufferCreateInfo& createInfo);
    ~VulkanFramebuffer();

    VulkanFramebuffer(const VulkanFramebuffer&) = delete;
    VulkanFramebuffer& operator=(const VulkanFramebuffer&) = delete;

    [[nodiscard]] VulkanDevice* GetDevice() const noexcept { return m_device.get(); }
    [[nodiscard]] const vk::detail::DispatchLoaderDynamic& GetDispatcher() const noexcept;
    [[nodiscard]] const vk::Framebuffer& GetHandle() const noexcept { return m_handle; }

    [[nodiscard]] const std::vector<rad::Ref<VulkanImageView>>& GetAttachments() const noexcept
    {
        return m_attachments;
    }
    [[nodiscard]] uint32_t GetWidth() const noexcept { return m_width; }
    [[nodiscard]] uint32_t GetHeight() const noexcept { return m_height; }
    [[nodiscard]] uint32_t GetLayers() const noexcept { return m_layers; }

private:
    rad::Ref<VulkanDevice> m_device;
    vk::Framebuffer m_handle = nullptr;
    std::vector<rad::Ref<VulkanImageView>> m_attachments;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_layers = 0;
}; // class VulkanFramebuffer

} // namespace sd
