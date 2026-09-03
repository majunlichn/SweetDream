#pragma once

#include <SweetDream/Core/API/Vulkan/VulkanCommon.h>

namespace sd
{

class VulkanDevice;

class VulkanRenderPass : public rad::RefCounted<VulkanRenderPass>
{
public:
    VulkanRenderPass(rad::Ref<VulkanDevice> device,
                     const vk::RenderPassCreateInfo& createInfo);
    ~VulkanRenderPass();

    VulkanRenderPass(const VulkanRenderPass&) = delete;
    VulkanRenderPass& operator=(const VulkanRenderPass&) = delete;

    [[nodiscard]] VulkanDevice* GetDevice() const noexcept { return m_device.get(); }
    [[nodiscard]] const vk::detail::DispatchLoaderDynamic& GetDispatcher() const noexcept;
    [[nodiscard]] const vk::RenderPass& GetHandle() const noexcept { return m_handle; }

private:
    rad::Ref<VulkanDevice> m_device;
    vk::RenderPass m_handle = nullptr;
}; // class VulkanRenderPass

} // namespace sd
