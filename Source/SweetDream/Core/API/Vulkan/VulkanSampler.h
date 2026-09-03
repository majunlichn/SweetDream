#pragma once

#include <SweetDream/Core/API/Vulkan/VulkanCommon.h>

namespace sd
{

class VulkanDevice;

class VulkanSampler : public rad::RefCounted<VulkanSampler>
{
public:
    VulkanSampler(rad::Ref<VulkanDevice> device,
                  const vk::SamplerCreateInfo& createInfo);
    ~VulkanSampler();

    VulkanSampler(const VulkanSampler&) = delete;
    VulkanSampler& operator=(const VulkanSampler&) = delete;

    [[nodiscard]] VulkanDevice* GetDevice() const noexcept { return m_device.get(); }
    [[nodiscard]] const vk::detail::DispatchLoaderDynamic& GetDispatcher() const noexcept;
    [[nodiscard]] const vk::Sampler& GetHandle() const noexcept { return m_handle; }

private:
    rad::Ref<VulkanDevice> m_device;
    vk::Sampler m_handle = nullptr;
}; // class VulkanSampler

} // namespace sd
