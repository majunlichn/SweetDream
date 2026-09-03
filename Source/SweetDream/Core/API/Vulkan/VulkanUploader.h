#pragma once

#include <SweetDream/Core/API/Vulkan/VulkanDevice.h>

namespace sd
{

class VulkanBuffer;
class VulkanCommandBuffer;
class VulkanCommandPool;
class VulkanFence;

class VulkanUploader : public rad::RefCounted<VulkanUploader>
{
public:
    ~VulkanUploader();

    VulkanUploader(const VulkanUploader&) = delete;
    VulkanUploader& operator=(const VulkanUploader&) = delete;

    [[nodiscard]] VulkanDevice* GetDevice() const noexcept { return m_device.get(); }

    void UploadBuffer(VulkanBuffer* buffer, const void* data,
                      vk::DeviceSize size, vk::DeviceSize offset = 0);
    void SubmitAndWait();

private:
    friend class VulkanDevice;

    explicit VulkanUploader(rad::Ref<VulkanDevice> device);

    void Begin();
    void End();
    [[nodiscard]] VulkanBuffer* AcquireStagingBuffer(vk::DeviceSize size);

    rad::Ref<VulkanDevice> m_device;
    rad::Ref<VulkanCommandPool> m_commandPool;
    rad::Ref<VulkanCommandBuffer> m_commandBuffer;
    rad::Ref<VulkanFence> m_fence;
    std::vector<rad::Ref<VulkanBuffer>> m_stagingBuffers;
    std::vector<vk::BufferMemoryBarrier2> m_bufferBarriers;
    std::vector<vk::ImageMemoryBarrier2> m_imageBarriers;
    std::size_t m_activeStagingBufferCount = 0;
}; // class VulkanUploader

} // namespace sd
