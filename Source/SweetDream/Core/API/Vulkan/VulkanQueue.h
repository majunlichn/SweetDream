#pragma once

#include <SweetDream/Core/API/Vulkan/VulkanCommon.h>

namespace sd
{

class VulkanCommandBuffer;
class VulkanDevice;
class VulkanFence;
class VulkanSemaphore;
class VulkanSwapchain;

class VulkanQueue
{
public:
    VulkanQueue(const VulkanQueue&) = delete;
    VulkanQueue& operator=(const VulkanQueue&) = delete;

    [[nodiscard]] VulkanDevice* GetDevice() const noexcept { return m_device; }
    [[nodiscard]] const vk::detail::DispatchLoaderDynamic& GetDispatcher() const noexcept;
    [[nodiscard]] const vk::Queue& GetHandle() const noexcept { return m_handle; }
    [[nodiscard]] uint32_t GetFamilyIndex() const noexcept { return m_familyIndex; }
    [[nodiscard]] uint32_t GetQueueIndex() const noexcept { return m_queueIndex; }

    // Queue operations must be externally synchronized.
    void Submit(vk::ArrayProxy<const vk::SubmitInfo> submits, vk::Fence fence = {});
    void Submit(VulkanCommandBuffer* commandBuffer, VulkanFence* fence = nullptr);
    void Submit(VulkanCommandBuffer* commandBuffer, VulkanSemaphore* waitSemaphore,
                vk::PipelineStageFlags waitStage, VulkanSemaphore* signalSemaphore,
                VulkanFence* fence = nullptr);
    void Submit2(vk::ArrayProxy<const vk::SubmitInfo2> submits, vk::Fence fence = {});
    void BindSparse(vk::ArrayProxy<const vk::BindSparseInfo> bindInfos,
                    vk::Fence fence = {});

    [[nodiscard]] vk::Result Present(const vk::PresentInfoKHR& presentInfo);
    [[nodiscard]] vk::Result Present(VulkanSwapchain* swapchain, uint32_t imageIndex,
                                     VulkanSemaphore* waitSemaphore = nullptr);
    void WaitIdle();

private:
    friend class VulkanDevice;

    VulkanQueue(VulkanDevice* device, vk::Queue handle, uint32_t familyIndex,
                uint32_t queueIndex);

    VulkanDevice* m_device = nullptr;
    vk::Queue m_handle = nullptr;
    uint32_t m_familyIndex = VK_QUEUE_FAMILY_IGNORED;
    uint32_t m_queueIndex = 0;
}; // class VulkanQueue

} // namespace sd
