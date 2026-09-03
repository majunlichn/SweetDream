#pragma once

#include <SweetDream/Core/API/Vulkan/VulkanCommon.h>

namespace sd
{

class VulkanDevice;

class VulkanEvent : public rad::RefCounted<VulkanEvent>
{
public:
    VulkanEvent(rad::Ref<VulkanDevice> device, const vk::EventCreateInfo& createInfo);
    ~VulkanEvent();

    VulkanEvent(const VulkanEvent&) = delete;
    VulkanEvent& operator=(const VulkanEvent&) = delete;

    [[nodiscard]] VulkanDevice* GetDevice() const noexcept { return m_device.get(); }
    [[nodiscard]] const vk::detail::DispatchLoaderDynamic& GetDispatcher() const noexcept;
    [[nodiscard]] const vk::Event& GetHandle() const noexcept { return m_handle; }

    [[nodiscard]] vk::Result GetStatus() const;

    void Set();
    void Reset();

private:
    rad::Ref<VulkanDevice> m_device;
    vk::Event m_handle = nullptr;
}; // class VulkanEvent

} // namespace sd
