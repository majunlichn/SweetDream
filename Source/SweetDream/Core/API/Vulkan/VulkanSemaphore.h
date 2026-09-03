#pragma once

#include <SweetDream/Core/API/Vulkan/VulkanCommon.h>

namespace sd
{

class VulkanDevice;

class VulkanSemaphore : public rad::RefCounted<VulkanSemaphore>
{
public:
    VulkanSemaphore(rad::Ref<VulkanDevice> device,
                    const vk::SemaphoreCreateInfo& createInfo);
    ~VulkanSemaphore();

    VulkanSemaphore(const VulkanSemaphore&) = delete;
    VulkanSemaphore& operator=(const VulkanSemaphore&) = delete;

    [[nodiscard]] VulkanDevice* GetDevice() const noexcept { return m_device.get(); }
    [[nodiscard]] const vk::detail::DispatchLoaderDynamic& GetDispatcher() const noexcept;
    [[nodiscard]] const vk::Semaphore& GetHandle() const noexcept { return m_handle; }

private:
    rad::Ref<VulkanDevice> m_device;
    vk::Semaphore m_handle = nullptr;
}; // class VulkanSemaphore

} // namespace sd
