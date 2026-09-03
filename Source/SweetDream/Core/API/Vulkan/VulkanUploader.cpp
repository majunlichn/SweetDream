#include <SweetDream/Core/API/Vulkan/VulkanUploader.h>
#include <SweetDream/Core/API/Vulkan/VulkanBuffer.h>
#include <SweetDream/Core/API/Vulkan/VulkanCommand.h>
#include <SweetDream/Core/API/Vulkan/VulkanFence.h>
#include <SweetDream/Core/API/Vulkan/VulkanQueue.h>

#include <stdexcept>
#include <utility>

namespace sd
{

VulkanUploader::VulkanUploader(rad::Ref<VulkanDevice> device) :
    m_device(std::move(device))
{
    if (!m_device)
    {
        throw std::invalid_argument("VulkanUploader requires a valid VulkanDevice");
    }
    if (!m_device->HasQueueFamily(VulkanQueueFamily::Graphics))
    {
        throw std::invalid_argument(
            "VulkanUploader requires a graphics-capable VulkanDevice");
    }

    m_commandPool = m_device->CreateCommandPool(
        VulkanQueueFamily::Graphics, vk::CommandPoolCreateFlagBits::eTransient |
                                         vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
    m_commandBuffer =
        m_commandPool->AllocateCommandBuffer(vk::CommandBufferLevel::ePrimary);
    m_fence = m_device->CreateFence();
    Begin();
}

VulkanUploader::~VulkanUploader() = default;

void VulkanUploader::Begin()
{
    m_commandBuffer->Reset();
    m_commandBuffer->Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    m_activeStagingBufferCount = 0;
    m_bufferBarriers.clear();
    m_imageBarriers.clear();
}

void VulkanUploader::End()
{
    if (!m_bufferBarriers.empty() || !m_imageBarriers.empty())
    {
        m_commandBuffer->PipelineBarrier(
            vk::ArrayProxy<const vk::MemoryBarrier2>{}, m_bufferBarriers,
            m_imageBarriers);
    }
    m_commandBuffer->End();
}

void VulkanUploader::UploadBuffer(VulkanBuffer* buffer, const void* data,
                                  vk::DeviceSize size,
                                  vk::DeviceSize offset)
{
    if (size == 0)
    {
        return;
    }
    if (buffer == nullptr)
    {
        throw std::invalid_argument(
            "Buffer upload requires a valid destination VulkanBuffer");
    }
    if (buffer->GetDevice() != m_device.get())
    {
        throw std::invalid_argument(
            "Upload destination belongs to a different VulkanDevice");
    }
    if (HasNoBits(buffer->GetUsage(), vk::BufferUsageFlagBits::eTransferDst))
    {
        throw std::invalid_argument(
            "Upload destination requires transfer-destination usage");
    }
    if (data == nullptr)
    {
        throw std::invalid_argument("Buffer upload requires source data");
    }
    if (offset > buffer->GetSize() || size > buffer->GetSize() - offset)
    {
        throw std::out_of_range(
            "Buffer upload range exceeds the destination size");
    }

    VulkanBuffer* stagingBuffer = AcquireStagingBuffer(size);
    stagingBuffer->WriteHost(data, size);

    const vk::BufferCopy copyRegion{0, offset, size};
    m_commandBuffer->CopyBuffer(stagingBuffer->GetHandle(),
                                buffer->GetHandle(), copyRegion);

    vk::BufferMemoryBarrier2 barrier;
    barrier.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
    barrier.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
    barrier.dstStageMask = vk::PipelineStageFlagBits2::eAllCommands;
    barrier.dstAccessMask = vk::AccessFlagBits2::eMemoryRead;
    // TODO: Transfer ownership when uploads are consumed by a non-graphics queue.
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = buffer->GetHandle();
    barrier.offset = offset;
    barrier.size = size;
    m_bufferBarriers.emplace_back(barrier);
}

void VulkanUploader::SubmitAndWait()
{
    const bool hasUploads = !m_bufferBarriers.empty() || !m_imageBarriers.empty();
    End();

    if (!hasUploads)
    {
        Begin();
        return;
    }

    m_fence->Reset();
    m_device->GetQueue(VulkanQueueFamily::Graphics)
        ->Submit(m_commandBuffer.get(), m_fence.get());
    if (m_fence->Wait() != vk::Result::eSuccess)
    {
        throw std::runtime_error("VulkanUploader submission did not complete");
    }
    Begin();
}

VulkanBuffer* VulkanUploader::AcquireStagingBuffer(vk::DeviceSize size)
{
    const std::size_t stagingIndex = m_activeStagingBufferCount++;
    if (stagingIndex == m_stagingBuffers.size())
    {
        m_stagingBuffers.emplace_back(m_device->CreateUploadBuffer(size));
    }
    else if (m_stagingBuffers[stagingIndex]->GetSize() < size)
    {
        m_stagingBuffers[stagingIndex] = m_device->CreateUploadBuffer(size);
    }
    return m_stagingBuffers[stagingIndex].get();
}

} // namespace sd
