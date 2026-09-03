#include <SweetDream/Core/API/Vulkan/VulkanDescriptor.h>
#include <SweetDream/Core/API/Vulkan/VulkanBuffer.h>
#include <SweetDream/Core/API/Vulkan/VulkanDevice.h>

#include <stdexcept>
#include <utility>

namespace sd
{

VulkanDescriptorSetLayout::VulkanDescriptorSetLayout(
    rad::Ref<VulkanDevice> device, const vk::DescriptorSetLayoutCreateInfo& createInfo) :
    m_device(std::move(device))
{
    if (!m_device)
    {
        throw std::invalid_argument("VulkanDescriptorSetLayout requires a valid VulkanDevice");
    }

    m_handle =
        m_device->GetHandle().createDescriptorSetLayout(createInfo, nullptr, GetDispatcher());
}

VulkanDescriptorSetLayout::~VulkanDescriptorSetLayout()
{
    if (m_handle)
    {
        m_device->GetHandle().destroyDescriptorSetLayout(m_handle, nullptr, GetDispatcher());
        m_handle = nullptr;
    }
}

const vk::detail::DispatchLoaderDynamic& VulkanDescriptorSetLayout::GetDispatcher() const noexcept
{
    return m_device->GetDispatcher();
}

VulkanDescriptorPool::VulkanDescriptorPool(rad::Ref<VulkanDevice> device,
                                           const vk::DescriptorPoolCreateInfo& createInfo) :
    m_device(std::move(device)),
    m_flags(createInfo.flags)
{
    if (!m_device)
    {
        throw std::invalid_argument("VulkanDescriptorPool requires a valid VulkanDevice");
    }

    m_handle = m_device->GetHandle().createDescriptorPool(createInfo, nullptr, GetDispatcher());
}

VulkanDescriptorPool::~VulkanDescriptorPool()
{
    if (m_handle)
    {
        m_device->GetHandle().destroyDescriptorPool(m_handle, nullptr, GetDispatcher());
        m_handle = nullptr;
    }
}

const vk::detail::DispatchLoaderDynamic& VulkanDescriptorPool::GetDispatcher() const noexcept
{
    return m_device->GetDispatcher();
}

std::vector<rad::Ref<VulkanDescriptorSet>> VulkanDescriptorPool::Allocate(
    vk::ArrayProxy<const vk::DescriptorSetLayout> layouts)
{
    if (layouts.empty())
    {
        throw std::invalid_argument("Descriptor set allocation requires at least one layout");
    }

    vk::DescriptorSetAllocateInfo allocateInfo;
    allocateInfo.descriptorPool = m_handle;
    allocateInfo.descriptorSetCount = layouts.size();
    allocateInfo.pSetLayouts = layouts.data();

    const std::vector<vk::DescriptorSet> handles =
        m_device->GetHandle().allocateDescriptorSets(allocateInfo, GetDispatcher());

    std::vector<rad::Ref<VulkanDescriptorSet>> descriptorSets;
    descriptorSets.reserve(handles.size());
    for (vk::DescriptorSet handle : handles)
    {
        descriptorSets.emplace_back(new VulkanDescriptorSet(this, handle));
    }
    return descriptorSets;
}

void VulkanDescriptorPool::Reset(vk::DescriptorPoolResetFlags flags)
{
    m_device->GetHandle().resetDescriptorPool(m_handle, flags, GetDispatcher());
}

VulkanDescriptorSet::VulkanDescriptorSet(rad::Ref<VulkanDescriptorPool> pool,
                                         vk::DescriptorSet handle) :
    m_pool(std::move(pool)),
    m_handle(handle)
{
    if (!m_pool)
    {
        throw std::invalid_argument("VulkanDescriptorSet requires a valid VulkanDescriptorPool");
    }
    if (!m_handle)
    {
        throw std::invalid_argument("VulkanDescriptorSet requires a valid descriptor set");
    }
}

VulkanDescriptorSet::~VulkanDescriptorSet()
{
    if (m_handle)
    {
        if (HasAnyBits(m_pool->GetFlags(),
                       vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet))
        {
            const vk::Result result = GetDevice()->GetHandle().freeDescriptorSets(
                m_pool->GetHandle(), 1, &m_handle, GetDispatcher());
            if (rad::UnderlyingCast(result) < 0)
            {
                SD_LOG(err, "vkFreeDescriptorSets failed: {}", vk::to_string(result));
            }
        }
        m_handle = nullptr;
    }
}

VulkanDevice* VulkanDescriptorSet::GetDevice() const noexcept
{
    return m_pool->GetDevice();
}

const vk::detail::DispatchLoaderDynamic& VulkanDescriptorSet::GetDispatcher() const noexcept
{
    return m_pool->GetDispatcher();
}

void VulkanDescriptorSet::WriteBuffer(
    uint32_t binding, vk::DescriptorType descriptorType,
    const VulkanBuffer* buffer, vk::DeviceSize offset, vk::DeviceSize range,
    uint32_t arrayElement)
{
    if (buffer == nullptr)
    {
        throw std::invalid_argument(
            "Descriptor buffer write requires a valid VulkanBuffer");
    }
    if (buffer->GetDevice() != GetDevice())
    {
        throw std::invalid_argument(
            "Descriptor buffer belongs to a different VulkanDevice");
    }
    vk::BufferUsageFlagBits requiredUsage;
    switch (descriptorType)
    {
    case vk::DescriptorType::eUniformBuffer:
    case vk::DescriptorType::eUniformBufferDynamic:
        requiredUsage = vk::BufferUsageFlagBits::eUniformBuffer;
        break;
    case vk::DescriptorType::eStorageBuffer:
    case vk::DescriptorType::eStorageBufferDynamic:
        requiredUsage = vk::BufferUsageFlagBits::eStorageBuffer;
        break;
    default:
        throw std::invalid_argument(
            "Descriptor type does not accept buffer descriptor info");
    }
    if (HasNoBits(buffer->GetUsage(), requiredUsage))
    {
        throw std::invalid_argument(
            "Buffer usage is incompatible with the descriptor type");
    }
    if (offset >= buffer->GetSize())
    {
        throw std::out_of_range(
            "Descriptor buffer offset exceeds the buffer size");
    }
    if (range != vk::WholeSize &&
        (range == 0 || range > buffer->GetSize() - offset))
    {
        throw std::out_of_range(
            "Descriptor buffer range exceeds the buffer size");
    }

    const vk::DescriptorBufferInfo bufferInfo{buffer->GetHandle(), offset, range};
    vk::WriteDescriptorSet write;
    write.dstSet = m_handle;
    write.dstBinding = binding;
    write.dstArrayElement = arrayElement;
    write.descriptorCount = 1;
    write.descriptorType = descriptorType;
    write.pBufferInfo = &bufferInfo;
    GetDevice()->UpdateDescriptorSets(write);
}

} // namespace sd
