#pragma once

#include <SweetDream/Core/API/Vulkan/VulkanCommon.h>

namespace sd
{

class VulkanBuffer;
class VulkanDevice;
class VulkanDescriptorSet;

[[nodiscard]] constexpr vk::DescriptorSetLayoutBinding
MakeDescriptorSetLayoutBinding(uint32_t binding, vk::DescriptorType descriptorType,
                               vk::ShaderStageFlags stageFlags,
                               uint32_t descriptorCount = 1) noexcept
{
    return {binding, descriptorType, descriptorCount, stageFlags};
}

class VulkanDescriptorSetLayout : public rad::RefCounted<VulkanDescriptorSetLayout>
{
public:
    VulkanDescriptorSetLayout(rad::Ref<VulkanDevice> device,
                              const vk::DescriptorSetLayoutCreateInfo& createInfo);
    ~VulkanDescriptorSetLayout();

    VulkanDescriptorSetLayout(const VulkanDescriptorSetLayout&) = delete;
    VulkanDescriptorSetLayout& operator=(const VulkanDescriptorSetLayout&) = delete;

    [[nodiscard]] VulkanDevice* GetDevice() const noexcept { return m_device.get(); }
    [[nodiscard]] const vk::detail::DispatchLoaderDynamic& GetDispatcher() const noexcept;
    [[nodiscard]] const vk::DescriptorSetLayout& GetHandle() const noexcept { return m_handle; }

private:
    rad::Ref<VulkanDevice> m_device;
    vk::DescriptorSetLayout m_handle = nullptr;
}; // class VulkanDescriptorSetLayout

class VulkanDescriptorPool : public rad::RefCounted<VulkanDescriptorPool>
{
public:
    VulkanDescriptorPool(rad::Ref<VulkanDevice> device,
                         const vk::DescriptorPoolCreateInfo& createInfo);
    ~VulkanDescriptorPool();

    VulkanDescriptorPool(const VulkanDescriptorPool&) = delete;
    VulkanDescriptorPool& operator=(const VulkanDescriptorPool&) = delete;

    [[nodiscard]] VulkanDevice* GetDevice() const noexcept { return m_device.get(); }
    [[nodiscard]] const vk::detail::DispatchLoaderDynamic& GetDispatcher() const noexcept;
    [[nodiscard]] const vk::DescriptorPool& GetHandle() const noexcept { return m_handle; }
    [[nodiscard]] vk::DescriptorPoolCreateFlags GetFlags() const noexcept { return m_flags; }

    [[nodiscard]] std::vector<rad::Ref<VulkanDescriptorSet>> Allocate(
        vk::ArrayProxy<const vk::DescriptorSetLayout> layouts);

    void Reset(vk::DescriptorPoolResetFlags flags = {});

private:
    rad::Ref<VulkanDevice> m_device;
    vk::DescriptorPoolCreateFlags m_flags = {};
    vk::DescriptorPool m_handle = nullptr;
}; // class VulkanDescriptorPool

class VulkanDescriptorSet : public rad::RefCounted<VulkanDescriptorSet>
{
public:
    ~VulkanDescriptorSet();

    VulkanDescriptorSet(const VulkanDescriptorSet&) = delete;
    VulkanDescriptorSet& operator=(const VulkanDescriptorSet&) = delete;

    [[nodiscard]] VulkanDevice* GetDevice() const noexcept;
    [[nodiscard]] const vk::detail::DispatchLoaderDynamic& GetDispatcher() const noexcept;
    [[nodiscard]] VulkanDescriptorPool* GetDescriptorPool() const noexcept { return m_pool.get(); }
    [[nodiscard]] const vk::DescriptorSet& GetHandle() const noexcept { return m_handle; }

    void WriteBuffer(uint32_t binding, vk::DescriptorType descriptorType,
                     const VulkanBuffer* buffer, vk::DeviceSize offset = 0,
                     vk::DeviceSize range = vk::WholeSize,
                     uint32_t arrayElement = 0);

private:
    friend class VulkanDescriptorPool;

    VulkanDescriptorSet(rad::Ref<VulkanDescriptorPool> pool, vk::DescriptorSet handle);

    rad::Ref<VulkanDescriptorPool> m_pool;
    vk::DescriptorSet m_handle = nullptr;
}; // class VulkanDescriptorSet

} // namespace sd
