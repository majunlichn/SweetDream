#pragma once

#include <SweetDream/Core/API/Vulkan/VulkanCommon.h>

namespace sd
{

class VulkanDevice;
class VulkanBufferView;

class VulkanBuffer : public rad::RefCounted<VulkanBuffer>
{
public:
    VulkanBuffer(rad::Ref<VulkanDevice> device, const vk::BufferCreateInfo& bufferInfo,
                 const VmaAllocationCreateInfo& allocCreateInfo);
    ~VulkanBuffer();

    VulkanBuffer(const VulkanBuffer&) = delete;
    VulkanBuffer& operator=(const VulkanBuffer&) = delete;

    static rad::Ref<VulkanBuffer> Create(
        rad::Ref<VulkanDevice> device, vk::DeviceSize size, vk::BufferUsageFlags usage,
        VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_AUTO,
        VmaAllocationCreateFlags allocFlags = 0);

    [[nodiscard]] VulkanDevice* GetDevice() const noexcept { return m_device.get(); }
    [[nodiscard]] const vk::detail::DispatchLoaderDynamic& GetDispatcher() const noexcept;
    [[nodiscard]] vk::Buffer GetHandle() const noexcept { return m_handle; }

    [[nodiscard]] vk::DeviceSize GetSize() const noexcept { return m_size; }
    [[nodiscard]] vk::BufferUsageFlags GetUsage() const noexcept { return m_usage; }
    [[nodiscard]] void* GetMappedData() const noexcept
    {
        return m_allocInfo.pMappedData;
    }

    [[nodiscard]] bool IsHostVisible() const noexcept
    {
        return HasAnyBits(m_memPropFlags, vk::MemoryPropertyFlagBits::eHostVisible);
    }
    [[nodiscard]] bool IsHostCoherent() const noexcept
    {
        return HasAnyBits(m_memPropFlags, vk::MemoryPropertyFlagBits::eHostCoherent);
    }

    void* MapMemory();
    void UnmapMemory();
    void ReadHost(void* data, vk::DeviceSize size, vk::DeviceSize offset = 0);
    void WriteHost(const void* data, vk::DeviceSize size, vk::DeviceSize offset = 0);

    rad::Ref<VulkanBufferView> CreateView(
        vk::Format format, vk::DeviceSize offset = 0,
        vk::DeviceSize range = vk::WholeSize, vk::BufferViewCreateFlags flags = {});

private:
    rad::Ref<VulkanDevice> m_device;
    vk::Buffer m_handle = nullptr;
    vk::DeviceSize m_size = 0;
    vk::BufferUsageFlags m_usage;
    VmaAllocation m_alloc = nullptr;
    VmaAllocationInfo m_allocInfo = {};
    vk::MemoryPropertyFlags m_memPropFlags;
}; // class VulkanBuffer

class VulkanBufferView : public rad::RefCounted<VulkanBufferView>
{
public:
    VulkanBufferView(rad::Ref<VulkanBuffer> buffer,
                     const vk::BufferViewCreateInfo& createInfo);
    ~VulkanBufferView();

    VulkanBufferView(const VulkanBufferView&) = delete;
    VulkanBufferView& operator=(const VulkanBufferView&) = delete;

    [[nodiscard]] VulkanDevice* GetDevice() const noexcept
    {
        return m_buffer->GetDevice();
    }
    [[nodiscard]] const vk::detail::DispatchLoaderDynamic& GetDispatcher() const noexcept;
    [[nodiscard]] VulkanBuffer* GetBuffer() const noexcept { return m_buffer.get(); }
    [[nodiscard]] vk::BufferView GetHandle() const noexcept { return m_handle; }

private:
    rad::Ref<VulkanBuffer> m_buffer;
    vk::BufferView m_handle = nullptr;
    vk::Format m_format = vk::Format::eUndefined;
    vk::DeviceSize m_offset = 0;
    vk::DeviceSize m_range = 0;
}; // class VulkanBufferView

} // namespace sd
