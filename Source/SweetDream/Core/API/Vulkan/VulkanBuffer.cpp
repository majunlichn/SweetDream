#include <SweetDream/Core/API/Vulkan/VulkanBuffer.h>
#include <SweetDream/Core/API/Vulkan/VulkanDevice.h>

#include <cstring>
#include <stdexcept>
#include <utility>

namespace sd
{

VulkanBuffer::VulkanBuffer(rad::Ref<VulkanDevice> device,
                           const vk::BufferCreateInfo& bufferInfo,
                           const VmaAllocationCreateInfo& allocCreateInfo) :
    m_device(std::move(device)),
    m_size(bufferInfo.size),
    m_usage(bufferInfo.usage)
{
    if (!m_device)
    {
        throw std::invalid_argument("VulkanBuffer requires a valid VulkanDevice");
    }

    VkBuffer buffer = VK_NULL_HANDLE;
    SD_CHECK_VKRESULT(vmaCreateBuffer(
        m_device->GetAllocator(), reinterpret_cast<const VkBufferCreateInfo*>(&bufferInfo),
        &allocCreateInfo, &buffer, &m_alloc, &m_allocInfo));
    m_handle = buffer;

    VkMemoryPropertyFlags memoryPropertyFlags = 0;
    vmaGetAllocationMemoryProperties(m_device->GetAllocator(), m_alloc,
                                     &memoryPropertyFlags);
    m_memPropFlags = vk::MemoryPropertyFlags{memoryPropertyFlags};
}

VulkanBuffer::~VulkanBuffer()
{
    if (m_handle)
    {
        vmaDestroyBuffer(m_device->GetAllocator(), m_handle, m_alloc);
        m_handle = nullptr;
        m_alloc = nullptr;
        m_allocInfo = {};
    }
}

rad::Ref<VulkanBuffer> VulkanBuffer::Create(
    rad::Ref<VulkanDevice> device, vk::DeviceSize size, vk::BufferUsageFlags usage,
    VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags allocFlags)
{
    vk::BufferCreateInfo bufferInfo;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;

    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.flags = allocFlags;
    allocCreateInfo.usage = memoryUsage;

    return rad::Ref<VulkanBuffer>{
        new VulkanBuffer(std::move(device), bufferInfo, allocCreateInfo)};
}

const vk::detail::DispatchLoaderDynamic& VulkanBuffer::GetDispatcher() const noexcept
{
    return m_device->GetDispatcher();
}

void* VulkanBuffer::MapMemory()
{
    void* mappedMemory = nullptr;
    SD_CHECK_VKRESULT(vmaMapMemory(m_device->GetAllocator(), m_alloc, &mappedMemory));
    return mappedMemory;
}

void VulkanBuffer::UnmapMemory()
{
    vmaUnmapMemory(m_device->GetAllocator(), m_alloc);
}

void VulkanBuffer::ReadHost(void* data, vk::DeviceSize size, vk::DeviceSize offset)
{
    if (size == 0)
    {
        return;
    }
    if (data == nullptr)
    {
        throw std::invalid_argument("VulkanBuffer::ReadHost requires a destination");
    }
    if (!IsHostVisible())
    {
        throw std::runtime_error("Cannot read a non-host-visible VulkanBuffer");
    }
    if (offset > m_size || size > m_size - offset)
    {
        throw std::out_of_range(
            "VulkanBuffer::ReadHost range exceeds the buffer size");
    }

    void* mappedMemory = MapMemory();
    VkResult invalidateResult = VK_SUCCESS;
    if (!IsHostCoherent())
    {
        invalidateResult =
            vmaInvalidateAllocation(m_device->GetAllocator(), m_alloc, offset, size);
    }
    if (invalidateResult == VK_SUCCESS)
    {
        std::memcpy(data, static_cast<const std::byte*>(mappedMemory) + offset,
                    static_cast<std::size_t>(size));
    }
    UnmapMemory();
    SD_CHECK_VKRESULT(invalidateResult);
}

void VulkanBuffer::WriteHost(const void* data, vk::DeviceSize size,
                             vk::DeviceSize offset)
{
    if (size == 0)
    {
        return;
    }
    if (data == nullptr)
    {
        throw std::invalid_argument("VulkanBuffer::WriteHost requires source data");
    }
    if (!IsHostVisible())
    {
        throw std::runtime_error("Cannot write a non-host-visible VulkanBuffer");
    }
    if (offset > m_size || size > m_size - offset)
    {
        throw std::out_of_range(
            "VulkanBuffer::WriteHost range exceeds the buffer size");
    }

    void* mappedMemory = MapMemory();
    std::memcpy(static_cast<std::byte*>(mappedMemory) + offset, data,
                static_cast<std::size_t>(size));
    VkResult flushResult = VK_SUCCESS;
    if (!IsHostCoherent())
    {
        flushResult =
            vmaFlushAllocation(m_device->GetAllocator(), m_alloc, offset, size);
    }
    UnmapMemory();
    SD_CHECK_VKRESULT(flushResult);
}

rad::Ref<VulkanBufferView> VulkanBuffer::CreateView(vk::Format format,
                                                    vk::DeviceSize offset,
                                                    vk::DeviceSize range,
                                                    vk::BufferViewCreateFlags flags)
{
    vk::BufferViewCreateInfo createInfo;
    createInfo.flags = flags;
    createInfo.buffer = m_handle;
    createInfo.format = format;
    createInfo.offset = offset;
    createInfo.range = range;

    return rad::Ref<VulkanBufferView>{
        new VulkanBufferView(rad::Ref<VulkanBuffer>{this}, createInfo)};
}

VulkanBufferView::VulkanBufferView(rad::Ref<VulkanBuffer> buffer,
                                   const vk::BufferViewCreateInfo& createInfo) :
    m_buffer(std::move(buffer)),
    m_format(createInfo.format),
    m_offset(createInfo.offset),
    m_range(createInfo.range)
{
    if (!m_buffer)
    {
        throw std::invalid_argument("VulkanBufferView requires a valid VulkanBuffer");
    }
    if (createInfo.buffer != m_buffer->GetHandle())
    {
        throw std::invalid_argument(
            "VulkanBufferView create info must reference the supplied VulkanBuffer");
    }

    m_handle = GetDevice()->GetHandle().createBufferView(createInfo, nullptr,
                                                         GetDispatcher());
}

VulkanBufferView::~VulkanBufferView()
{
    if (m_handle)
    {
        GetDevice()->GetHandle().destroyBufferView(m_handle, nullptr, GetDispatcher());
        m_handle = nullptr;
    }
}

const vk::detail::DispatchLoaderDynamic& VulkanBufferView::GetDispatcher() const noexcept
{
    return m_buffer->GetDispatcher();
}

} // namespace sd
