#include <SweetDream/Core/API/Vulkan/VulkanQueue.h>

#include <SweetDream/Core/API/Vulkan/VulkanCommand.h>
#include <SweetDream/Core/API/Vulkan/VulkanDevice.h>
#include <SweetDream/Core/API/Vulkan/VulkanFence.h>
#include <SweetDream/Core/API/Vulkan/VulkanSemaphore.h>
#include <SweetDream/Core/API/Vulkan/VulkanSwapchain.h>

#include <stdexcept>

namespace sd
{

VulkanQueue::VulkanQueue(VulkanDevice* device, vk::Queue handle, uint32_t familyIndex,
                         uint32_t queueIndex) :
    m_device(device),
    m_handle(handle),
    m_familyIndex(familyIndex),
    m_queueIndex(queueIndex)
{
    if (m_device == nullptr)
    {
        throw std::invalid_argument("VulkanQueue requires a valid VulkanDevice");
    }
    if (!m_handle)
    {
        throw std::invalid_argument("VulkanQueue requires a valid queue");
    }
}

const vk::detail::DispatchLoaderDynamic& VulkanQueue::GetDispatcher() const noexcept
{
    return m_device->GetDispatcher();
}

void VulkanQueue::Submit(vk::ArrayProxy<const vk::SubmitInfo> submits, vk::Fence fence)
{
    m_handle.submit(submits, fence, GetDispatcher());
}

void VulkanQueue::Submit(VulkanCommandBuffer* commandBuffer, VulkanFence* fence)
{
    if (commandBuffer == nullptr || commandBuffer->GetDevice() != m_device)
    {
        throw std::invalid_argument(
            "VulkanQueue::Submit requires a command buffer from the same device");
    }
    if (fence != nullptr && fence->GetDevice() != m_device)
    {
        throw std::invalid_argument(
            "VulkanQueue::Submit requires a fence from the same device");
    }

    const vk::CommandBuffer commandBufferHandle = commandBuffer->GetHandle();
    vk::SubmitInfo submitInfo;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBufferHandle;
    Submit(submitInfo, fence != nullptr ? fence->GetHandle() : vk::Fence{});
}

void VulkanQueue::Submit(VulkanCommandBuffer* commandBuffer,
                         VulkanSemaphore* waitSemaphore,
                         vk::PipelineStageFlags waitStage,
                         VulkanSemaphore* signalSemaphore, VulkanFence* fence)
{
    if (commandBuffer == nullptr || commandBuffer->GetDevice() != m_device)
    {
        throw std::invalid_argument(
            "VulkanQueue::Submit requires a command buffer from the same device");
    }
    if (waitSemaphore == nullptr || waitSemaphore->GetDevice() != m_device)
    {
        throw std::invalid_argument(
            "VulkanQueue::Submit requires a wait semaphore from the same device");
    }
    if (signalSemaphore == nullptr || signalSemaphore->GetDevice() != m_device)
    {
        throw std::invalid_argument(
            "VulkanQueue::Submit requires a signal semaphore from the same device");
    }
    if (fence != nullptr && fence->GetDevice() != m_device)
    {
        throw std::invalid_argument(
            "VulkanQueue::Submit requires a fence from the same device");
    }

    const vk::Semaphore waitSemaphoreHandle = waitSemaphore->GetHandle();
    const vk::Semaphore signalSemaphoreHandle = signalSemaphore->GetHandle();
    const vk::CommandBuffer commandBufferHandle = commandBuffer->GetHandle();

    vk::SubmitInfo submitInfo;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &waitSemaphoreHandle;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBufferHandle;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &signalSemaphoreHandle;
    Submit(submitInfo, fence != nullptr ? fence->GetHandle() : vk::Fence{});
}

void VulkanQueue::Submit2(vk::ArrayProxy<const vk::SubmitInfo2> submits,
                          vk::Fence fence)
{
    m_handle.submit2(submits, fence, GetDispatcher());
}

void VulkanQueue::BindSparse(vk::ArrayProxy<const vk::BindSparseInfo> bindInfos,
                             vk::Fence fence)
{
    m_handle.bindSparse(bindInfos, fence, GetDispatcher());
}

vk::Result VulkanQueue::Present(const vk::PresentInfoKHR& presentInfo)
{
    const VkResult result = GetDispatcher().vkQueuePresentKHR(
        static_cast<VkQueue>(m_handle),
        reinterpret_cast<const VkPresentInfoKHR*>(&presentInfo));
    return static_cast<vk::Result>(result);
}

vk::Result VulkanQueue::Present(VulkanSwapchain* swapchain, uint32_t imageIndex,
                                VulkanSemaphore* waitSemaphore)
{
    if (swapchain == nullptr || swapchain->GetDevice() != m_device)
    {
        throw std::invalid_argument(
            "VulkanQueue::Present requires a swapchain from the same device");
    }
    if (imageIndex >= swapchain->GetImageCount())
    {
        throw std::out_of_range("VulkanQueue::Present image index is out of range");
    }
    if (waitSemaphore != nullptr && waitSemaphore->GetDevice() != m_device)
    {
        throw std::invalid_argument(
            "VulkanQueue::Present requires a semaphore from the same device");
    }

    const vk::Semaphore waitSemaphoreHandle =
        waitSemaphore != nullptr ? waitSemaphore->GetHandle() : vk::Semaphore{};
    const vk::SwapchainKHR swapchainHandle = swapchain->GetHandle();
    vk::PresentInfoKHR presentInfo;
    presentInfo.waitSemaphoreCount = waitSemaphore != nullptr ? 1 : 0;
    presentInfo.pWaitSemaphores =
        waitSemaphore != nullptr ? &waitSemaphoreHandle : nullptr;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchainHandle;
    presentInfo.pImageIndices = &imageIndex;
    return Present(presentInfo);
}

void VulkanQueue::WaitIdle()
{
    m_handle.waitIdle(GetDispatcher());
}

} // namespace sd
