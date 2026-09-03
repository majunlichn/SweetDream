#pragma once

#include <SweetDream/Core/Common/String.h>
#include <SweetDream/Core/API/Vulkan/VulkanInstance.h>

#include <array>
#include <memory>
#include <span>
#include <stdexcept>

namespace sd
{

class VulkanBuffer;
class VulkanCommandPool;
class VulkanComputePipeline;
class VulkanDescriptorPool;
class VulkanDescriptorSetLayout;
class VulkanEvent;
class VulkanFence;
class VulkanFramebuffer;
class VulkanGraphicsPipeline;
class VulkanImage;
class VulkanImageView;
class VulkanPipelineLayout;
class VulkanQueryPool;
class VulkanQueue;
class VulkanRenderPass;
class VulkanSampler;
class VulkanSemaphore;
class VulkanShaderModule;
class VulkanSurface;
class VulkanSwapchain;
class VulkanUploader;

struct VulkanComputePipelineCreateInfo;
struct VulkanFramebufferCreateInfo;
struct VulkanGraphicsPipelineCreateInfo;

enum class VulkanQueueFamily
{
    Graphics,
    Compute,
    Transfer,
    Count
};

class VulkanDevice : public rad::RefCounted<VulkanDevice>
{
public:
    // presentSurface may be null. When set, the graphics queue family prefers a
    // family that can also present to that surface (vkcube-style combined queue).
    VulkanDevice(rad::Ref<VulkanInstance> instance, vk::PhysicalDevice physicalDevice,
                 vk::SurfaceKHR presentSurface = {});
    ~VulkanDevice();

    VulkanDevice(const VulkanDevice&) = delete;
    VulkanDevice& operator=(const VulkanDevice&) = delete;

    [[nodiscard]] VulkanInstance* GetInstance() const noexcept { return m_instance.get(); }
    [[nodiscard]] vk::PhysicalDevice GetPhysicalDevice() const noexcept
    {
        return m_physicalDevice;
    }
    [[nodiscard]] const vk::Device& GetHandle() const noexcept { return m_handle; }
    [[nodiscard]] const vk::detail::DispatchLoaderDynamic& GetDispatcher() const noexcept
    {
        return m_dispatcher;
    }

    [[nodiscard]] PFN_vkVoidFunction GetProcAddr(const char* name) const
    {
        return m_handle.getProcAddr(name, m_dispatcher);
    }
    [[nodiscard]] VmaAllocator GetAllocator() const noexcept { return m_allocator; }

    [[nodiscard]] const char* GetName() const noexcept { return m_properties.deviceName; }
    [[nodiscard]] VulkanVersion GetApiVersion() const noexcept { return m_apiVersion; }
    [[nodiscard]] const vk::PhysicalDeviceProperties& GetProperties() const noexcept
    {
        return m_properties;
    }

    [[nodiscard]] vk::SurfaceCapabilitiesKHR GetCapabilities(vk::SurfaceKHR surface) const;
    [[nodiscard]] std::vector<vk::SurfaceFormatKHR> GetSurfaceFormats(
        vk::SurfaceKHR surface) const;
    [[nodiscard]] std::vector<vk::PresentModeKHR> GetPresentModes(
        vk::SurfaceKHR surface) const;
    [[nodiscard]] bool GetSurfaceSupport(vk::SurfaceKHR surface,
                                         uint32_t queueFamilyIndex) const;

    [[nodiscard]] uint32_t GetQueueFamilyIndex(VulkanQueueFamily queueFamily) const
    {
        return m_queueFamilyIndices.at(rad::UnderlyingCast(queueFamily));
    }

    [[nodiscard]] bool HasQueueFamily(VulkanQueueFamily queueFamily) const
    {
        const std::size_t slot =
            static_cast<std::size_t>(rad::UnderlyingCast(queueFamily));
        return slot < m_queueFamilyIndices.size() &&
               m_queueFamilyIndices[slot] != VK_QUEUE_FAMILY_IGNORED;
    }

    [[nodiscard]] const vk::QueueFamilyProperties& GetQueueFamilyProperties(
        VulkanQueueFamily queueFamily) const
    {
        if (!HasQueueFamily(queueFamily))
        {
            throw std::out_of_range("Requested queue family is unavailable");
        }
        return m_queueFamilyProperties[GetQueueFamilyIndex(queueFamily)];
    }

    [[nodiscard]] VulkanQueue* GetQueue(VulkanQueueFamily queueFamily) noexcept;

    [[nodiscard]] vk::Queue GetQueue(const vk::DeviceQueueInfo2& queueInfo)
    {
        return m_handle.getQueue2(queueInfo, m_dispatcher);
    }

    [[nodiscard]] bool IsExtensionEnabled(std::string_view name) const
    {
        return m_enabledExtensions.contains(name);
    }

    void WaitIdle();

    void UpdateDescriptorSets(
        vk::ArrayProxy<const vk::WriteDescriptorSet> writes,
        vk::ArrayProxy<const vk::CopyDescriptorSet> copies = {});

    [[nodiscard]] rad::Ref<VulkanSwapchain> CreateSwapchain(
        rad::Ref<VulkanSurface> surface,
        const vk::SwapchainCreateInfoKHR& createInfo);

    [[nodiscard]] rad::Ref<VulkanBuffer> CreateBuffer(
        const vk::BufferCreateInfo& createInfo,
        const VmaAllocationCreateInfo& allocationCreateInfo);
    [[nodiscard]] rad::Ref<VulkanBuffer> CreateBuffer(
        vk::DeviceSize size, vk::BufferUsageFlags usage,
        VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_AUTO,
        VmaAllocationCreateFlags allocationFlags = 0);
    // Uniform and upload buffers are host-visible and coherent.
    [[nodiscard]] rad::Ref<VulkanBuffer> CreateUniformBuffer(
        vk::DeviceSize size, vk::BufferUsageFlags additionalUsage = {});
    [[nodiscard]] rad::Ref<VulkanBuffer> CreateUploadBuffer(
        vk::DeviceSize size, vk::BufferUsageFlags additionalUsage = {});
    // Vertex, index, and storage buffers prefer device-local memory.
    [[nodiscard]] rad::Ref<VulkanBuffer> CreateVertexBuffer(
        vk::DeviceSize size, vk::BufferUsageFlags additionalUsage = {});
    [[nodiscard]] rad::Ref<VulkanBuffer> CreateIndexBuffer(
        vk::DeviceSize size, vk::BufferUsageFlags additionalUsage = {});
    [[nodiscard]] rad::Ref<VulkanBuffer> CreateStorageBuffer(
        vk::DeviceSize size, vk::BufferUsageFlags additionalUsage = {});

    [[nodiscard]] rad::Ref<VulkanImage> CreateImage(
        const vk::ImageCreateInfo& createInfo,
        const VmaAllocationCreateInfo& allocationCreateInfo);
    [[nodiscard]] rad::Ref<VulkanImage> CreateImage2D(
        uint32_t width, uint32_t height, vk::Format format,
        vk::ImageUsageFlags usage,
        VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_AUTO,
        VmaAllocationCreateFlags allocationFlags = 0, uint32_t mipLevels = 1,
        uint32_t arrayLayers = 1,
        vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1,
        vk::ImageTiling tiling = vk::ImageTiling::eOptimal);

    [[nodiscard]] rad::Ref<VulkanSampler> CreateSampler(
        const vk::SamplerCreateInfo& createInfo);
    [[nodiscard]] rad::Ref<VulkanSampler> CreateSampler(
        vk::Filter filter = vk::Filter::eLinear,
        vk::SamplerAddressMode addressMode = vk::SamplerAddressMode::eRepeat,
        vk::SamplerMipmapMode mipmapMode = vk::SamplerMipmapMode::eLinear,
        float maxLod = VK_LOD_CLAMP_NONE);

    [[nodiscard]] rad::Ref<VulkanFence> CreateFence(
        const vk::FenceCreateInfo& createInfo);
    [[nodiscard]] rad::Ref<VulkanFence> CreateFence(bool signaled = false);
    [[nodiscard]] rad::Ref<VulkanSemaphore> CreateSemaphore(
        const vk::SemaphoreCreateInfo& createInfo);
    [[nodiscard]] rad::Ref<VulkanSemaphore> CreateSemaphore();
    [[nodiscard]] rad::Ref<VulkanEvent> CreateEvent(
        const vk::EventCreateInfo& createInfo);
    [[nodiscard]] rad::Ref<VulkanEvent> CreateEvent();

    [[nodiscard]] rad::Ref<VulkanCommandPool> CreateCommandPool(
        VulkanQueueFamily queueFamily,
        vk::CommandPoolCreateFlags flags = {});
    [[nodiscard]] rad::Ref<VulkanUploader> CreateUploader();
    [[nodiscard]] rad::Ref<VulkanQueryPool> CreateQueryPool(
        const vk::QueryPoolCreateInfo& createInfo);
    [[nodiscard]] rad::Ref<VulkanQueryPool> CreateQueryPool(
        vk::QueryType queryType, uint32_t queryCount,
        vk::QueryPipelineStatisticFlags pipelineStatistics = {},
        vk::QueryPoolCreateFlags flags = {});

    [[nodiscard]] rad::Ref<VulkanRenderPass> CreateRenderPass(
        const vk::RenderPassCreateInfo& createInfo);
    [[nodiscard]] rad::Ref<VulkanRenderPass> CreateRenderPass(
        vk::ArrayProxy<const vk::AttachmentDescription> attachments,
        vk::ArrayProxy<const vk::SubpassDescription> subpasses,
        vk::ArrayProxy<const vk::SubpassDependency> dependencies = {},
        vk::RenderPassCreateFlags flags = {});
    [[nodiscard]] rad::Ref<VulkanFramebuffer> CreateFramebuffer(
        const VulkanFramebufferCreateInfo& createInfo);
    [[nodiscard]] rad::Ref<VulkanFramebuffer> CreateFramebuffer(
        rad::Ref<VulkanRenderPass> renderPass,
        std::vector<rad::Ref<VulkanImageView>> attachments, uint32_t width,
        uint32_t height, uint32_t layers = 1);

    [[nodiscard]] rad::Ref<VulkanShaderModule> CreateShaderModule(
        const vk::ShaderModuleCreateInfo& createInfo);
    [[nodiscard]] rad::Ref<VulkanShaderModule> CreateShaderModule(
        std::span<const uint32_t> spirv);
    [[nodiscard]] rad::Ref<VulkanPipelineLayout> CreatePipelineLayout(
        const vk::PipelineLayoutCreateInfo& createInfo);
    [[nodiscard]] rad::Ref<VulkanPipelineLayout> CreatePipelineLayout(
        vk::ArrayProxy<const vk::DescriptorSetLayout> setLayouts,
        vk::ArrayProxy<const vk::PushConstantRange> pushConstantRanges = {},
        vk::PipelineLayoutCreateFlags flags = {});
    [[nodiscard]] rad::Ref<VulkanPipelineLayout> CreatePipelineLayout(
        std::span<const rad::Ref<VulkanDescriptorSetLayout>> setLayouts,
        vk::ArrayProxy<const vk::PushConstantRange> pushConstantRanges = {},
        vk::PipelineLayoutCreateFlags flags = {});
    [[nodiscard]] rad::Ref<VulkanGraphicsPipeline> CreateGraphicsPipeline(
        const VulkanGraphicsPipelineCreateInfo& createInfo);
    [[nodiscard]] rad::Ref<VulkanComputePipeline> CreateComputePipeline(
        const VulkanComputePipelineCreateInfo& createInfo);
    [[nodiscard]] rad::Ref<VulkanComputePipeline> CreateComputePipeline(
        rad::Ref<VulkanShaderModule> shaderModule,
        rad::Ref<VulkanPipelineLayout> layout,
        std::string_view entryPoint = "main");

    [[nodiscard]] rad::Ref<VulkanDescriptorSetLayout> CreateDescriptorSetLayout(
        const vk::DescriptorSetLayoutCreateInfo& createInfo);
    [[nodiscard]] rad::Ref<VulkanDescriptorSetLayout> CreateDescriptorSetLayout(
        vk::ArrayProxy<const vk::DescriptorSetLayoutBinding> bindings,
        vk::DescriptorSetLayoutCreateFlags flags = {});
    [[nodiscard]] rad::Ref<VulkanDescriptorPool> CreateDescriptorPool(
        const vk::DescriptorPoolCreateInfo& createInfo);
    [[nodiscard]] rad::Ref<VulkanDescriptorPool> CreateDescriptorPool(
        uint32_t maxSets, vk::ArrayProxy<const vk::DescriptorPoolSize> poolSizes,
        vk::DescriptorPoolCreateFlags flags = {});

private:
    void QueryPropertiesAndFeatures();
    void SelectQueueFamilies(vk::SurfaceKHR presentSurface);
    void InitializeQueues();

    rad::Ref<VulkanInstance> m_instance;
    vk::PhysicalDevice m_physicalDevice;
    vk::Device m_handle = nullptr;
    vk::detail::DispatchLoaderDynamic m_dispatcher;
    std::array<uint32_t, rad::UnderlyingCast(VulkanQueueFamily::Count)> m_queueFamilyIndices;
    std::array<std::unique_ptr<VulkanQueue>,
               rad::UnderlyingCast(VulkanQueueFamily::Count)>
        m_queues;
    VmaAllocator m_allocator = nullptr;
    StringSet m_enabledExtensions;

    VulkanVersion m_apiVersion = VK_API_VERSION_1_0;
    vk::PhysicalDeviceProperties m_properties = {};
    vk::PhysicalDeviceProperties2 m_properties2 = {};
    vk::PhysicalDeviceDriverProperties m_driverProperties = {};
    vk::PhysicalDeviceVulkan11Properties m_vk11Properties = {};
    vk::PhysicalDeviceVulkan12Properties m_vk12Properties = {};
    vk::PhysicalDeviceVulkan13Properties m_vk13Properties = {};

    std::vector<vk::QueueFamilyProperties> m_queueFamilyProperties = {};
    vk::PhysicalDeviceMemoryProperties m_memoryProperties = {};

    vk::PhysicalDeviceFeatures m_features = {};
    vk::PhysicalDeviceFeatures2 m_features2 = {};
    vk::PhysicalDeviceVulkan11Features m_Vulkan11Features = {};
    vk::PhysicalDeviceVulkan12Features m_Vulkan12Features = {};
    vk::PhysicalDeviceVulkan13Features m_Vulkan13Features = {};
    vk::PhysicalDeviceVulkan14Features m_Vulkan14Features = {};
}; // class VulkanDevice

} // namespace sd
