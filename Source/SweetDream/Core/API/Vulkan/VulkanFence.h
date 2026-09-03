#pragma once

#include <SweetDream/Core/API/Vulkan/VulkanCommon.h>

namespace sd
{

class VulkanDevice;

class VulkanFence : public rad::RefCounted<VulkanFence>
{
public:
    VulkanFence(rad::Ref<VulkanDevice> device, const vk::FenceCreateInfo& createInfo);
    ~VulkanFence();

    VulkanFence(const VulkanFence&) = delete;
    VulkanFence& operator=(const VulkanFence&) = delete;

    [[nodiscard]] VulkanDevice* GetDevice() const noexcept { return m_device.get(); }
    [[nodiscard]] const vk::detail::DispatchLoaderDynamic& GetDispatcher() const noexcept;
    [[nodiscard]] const vk::Fence& GetHandle() const noexcept { return m_handle; }

    // timeout is in nanoseconds and is adjusted to the closest implementation-supported value.
    [[nodiscard]] vk::Result Wait(uint64_t timeout = UINT64_MAX);
    void Reset();

private:
    rad::Ref<VulkanDevice> m_device;
    vk::Fence m_handle = nullptr;
}; // class VulkanFence

} // namespace sd
