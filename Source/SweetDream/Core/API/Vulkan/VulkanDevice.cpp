#include <SweetDream/Core/API/Vulkan/VulkanDevice.h>
#include <SweetDream/Core/API/Vulkan/VulkanBuffer.h>
#include <SweetDream/Core/API/Vulkan/VulkanCommand.h>
#include <SweetDream/Core/API/Vulkan/VulkanDescriptor.h>
#include <SweetDream/Core/API/Vulkan/VulkanEvent.h>
#include <SweetDream/Core/API/Vulkan/VulkanFence.h>
#include <SweetDream/Core/API/Vulkan/VulkanFramebuffer.h>
#include <SweetDream/Core/API/Vulkan/VulkanImage.h>
#include <SweetDream/Core/API/Vulkan/VulkanPipeline.h>
#include <SweetDream/Core/API/Vulkan/VulkanQuery.h>
#include <SweetDream/Core/API/Vulkan/VulkanQueue.h>
#include <SweetDream/Core/API/Vulkan/VulkanRenderPass.h>
#include <SweetDream/Core/API/Vulkan/VulkanSampler.h>
#include <SweetDream/Core/API/Vulkan/VulkanSemaphore.h>
#include <SweetDream/Core/API/Vulkan/VulkanSurface.h>
#include <SweetDream/Core/API/Vulkan/VulkanSwapchain.h>
#include <SweetDream/Core/API/Vulkan/VulkanUploader.h>

#include <algorithm>
#include <exception>
#include <stdexcept>
#include <utility>

namespace sd
{

VulkanDevice::VulkanDevice(rad::Ref<VulkanInstance> instance, vk::PhysicalDevice physicalDevice,
                           vk::SurfaceKHR presentSurface) :
    m_instance(std::move(instance)),
    m_physicalDevice(physicalDevice)
{
    m_queueFamilyIndices.fill(VK_QUEUE_FAMILY_IGNORED);

    if (!m_instance)
    {
        throw std::invalid_argument("VulkanDevice requires a valid VulkanInstance");
    }
    if (!m_physicalDevice)
    {
        throw std::invalid_argument("VulkanDevice requires a valid physical device");
    }

    try
    {
        QueryPropertiesAndFeatures();
        SelectQueueFamilies(presentSurface);

        const auto availableExtensions =
            m_physicalDevice.enumerateDeviceExtensionProperties(nullptr,
                                                                 m_instance->GetDispatcher());
        if (m_instance->IsExtensionEnabled(VK_KHR_SURFACE_EXTENSION_NAME) &&
            Contains(availableExtensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME))
        {
            m_enabledExtensions.emplace(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        }
        constexpr std::string_view PortabilitySubsetExtension = "VK_KHR_portability_subset";
        if (Contains(availableExtensions, PortabilitySubsetExtension))
        {
            m_enabledExtensions.emplace(PortabilitySubsetExtension);
        }

        std::set<uint32_t> uniqueQueueFamilies;
        for (const uint32_t queueFamily : m_queueFamilyIndices)
        {
            if (queueFamily != VK_QUEUE_FAMILY_IGNORED)
            {
                uniqueQueueFamilies.emplace(queueFamily);
            }
        }

        constexpr float queuePriority = 1.0f;
        std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
        queueCreateInfos.reserve(uniqueQueueFamilies.size());
        for (const uint32_t queueFamily : uniqueQueueFamilies)
        {
            vk::DeviceQueueCreateInfo queueCreateInfo;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        std::vector<const char*> extensionNames;
        extensionNames.reserve(m_enabledExtensions.size());
        for (const std::string& extension : m_enabledExtensions)
        {
            extensionNames.push_back(extension.c_str());
        }

        vk::DeviceCreateInfo createInfo;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensionNames.size());
        createInfo.ppEnabledExtensionNames = extensionNames.data();
        if (m_apiVersion.IsGreaterEqualThan(VK_API_VERSION_1_1))
        {
            // Features were queried into m_features2 (+ Vulkan 1.1–1.4 feature structs).
            createInfo.pNext = &m_features2;
        }
        else
        {
            createInfo.pEnabledFeatures = &m_features;
        }

        m_handle = m_physicalDevice.createDevice(createInfo, nullptr,
                                                  m_instance->GetDispatcher());
        // Initialize in two stages instead of relying on the legacy overload whose
        // vkGetDeviceProcAddr argument changed between Vulkan-Hpp versions.
        m_dispatcher.init(m_instance->GetHandle(),
                          m_instance->GetDispatcher().vkGetInstanceProcAddr);
        m_dispatcher.init(m_handle);
        InitializeQueues();

        VmaVulkanFunctions vulkanFunctions = {};
        vulkanFunctions.vkGetInstanceProcAddr =
            m_instance->GetDispatcher().vkGetInstanceProcAddr;
        vulkanFunctions.vkGetDeviceProcAddr = m_dispatcher.vkGetDeviceProcAddr;

        VmaAllocatorCreateInfo allocatorCreateInfo = {};
        allocatorCreateInfo.physicalDevice = m_physicalDevice;
        allocatorCreateInfo.device = m_handle;
        allocatorCreateInfo.instance = m_instance->GetHandle();
        allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;
        allocatorCreateInfo.vulkanApiVersion = GetApiVersion();
        if (GetApiVersion().IsGreaterEqualThan(VK_API_VERSION_1_1))
        {
            allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT |
                                        VMA_ALLOCATOR_CREATE_KHR_BIND_MEMORY2_BIT;
        }

        SD_CHECK_VKRESULT(vmaCreateAllocator(&allocatorCreateInfo, &m_allocator));
    }
    catch (...)
    {
        if (m_allocator != nullptr)
        {
            vmaDestroyAllocator(m_allocator);
            m_allocator = nullptr;
        }
        if (m_handle)
        {
            m_handle.destroy(nullptr, m_dispatcher);
            m_handle = nullptr;
        }
        throw;
    }
}

VulkanDevice::~VulkanDevice()
{
    if (m_handle)
    {
        try
        {
            WaitIdle();
        }
        catch (const std::exception& exception)
        {
            SD_LOG(err, "Failed to wait for Vulkan device '{}': {}", GetName(),
                   exception.what());
        }
    }

    if (m_allocator != nullptr)
    {
        vmaDestroyAllocator(m_allocator);
        m_allocator = nullptr;
    }
    for (std::unique_ptr<VulkanQueue>& queue : m_queues)
    {
        queue.reset();
    }
    if (m_handle)
    {
        m_handle.destroy(nullptr, m_dispatcher);
        m_handle = nullptr;
    }
}

vk::SurfaceCapabilitiesKHR VulkanDevice::GetCapabilities(vk::SurfaceKHR surface) const
{
    return m_physicalDevice.getSurfaceCapabilitiesKHR(surface, m_instance->GetDispatcher());
}

std::vector<vk::SurfaceFormatKHR> VulkanDevice::GetSurfaceFormats(vk::SurfaceKHR surface) const
{
    return m_physicalDevice.getSurfaceFormatsKHR(surface, m_instance->GetDispatcher());
}

std::vector<vk::PresentModeKHR> VulkanDevice::GetPresentModes(vk::SurfaceKHR surface) const
{
    return m_physicalDevice.getSurfacePresentModesKHR(surface, m_instance->GetDispatcher());
}

bool VulkanDevice::GetSurfaceSupport(vk::SurfaceKHR surface, uint32_t queueFamilyIndex) const
{
    return m_physicalDevice.getSurfaceSupportKHR(queueFamilyIndex, surface,
                                                 m_instance->GetDispatcher());
}

VulkanQueue* VulkanDevice::GetQueue(VulkanQueueFamily queueFamily) noexcept
{
    if (!HasQueueFamily(queueFamily))
    {
        return nullptr;
    }
    return m_queues[rad::UnderlyingCast(queueFamily)].get();
}

void VulkanDevice::WaitIdle()
{
    if (m_handle)
    {
        m_handle.waitIdle(m_dispatcher);
    }
}

void VulkanDevice::InitializeQueues()
{
    for (std::size_t slot = 0; slot < m_queueFamilyIndices.size(); ++slot)
    {
        const uint32_t familyIndex = m_queueFamilyIndices[slot];
        if (familyIndex == VK_QUEUE_FAMILY_IGNORED)
        {
            continue;
        }

        const vk::Queue handle = m_handle.getQueue(familyIndex, 0, m_dispatcher);
        m_queues[slot] =
            std::unique_ptr<VulkanQueue>(new VulkanQueue(this, handle, familyIndex, 0));
    }
}

void VulkanDevice::UpdateDescriptorSets(
    vk::ArrayProxy<const vk::WriteDescriptorSet> writes,
    vk::ArrayProxy<const vk::CopyDescriptorSet> copies)
{
    m_handle.updateDescriptorSets(writes, copies, m_dispatcher);
}

rad::Ref<VulkanSwapchain> VulkanDevice::CreateSwapchain(
    rad::Ref<VulkanSurface> surface,
    const vk::SwapchainCreateInfoKHR& createInfo)
{
    return rad::Ref<VulkanSwapchain>{new VulkanSwapchain(
        rad::Ref<VulkanDevice>{this}, std::move(surface), createInfo)};
}

rad::Ref<VulkanBuffer> VulkanDevice::CreateBuffer(
    const vk::BufferCreateInfo& createInfo,
    const VmaAllocationCreateInfo& allocationCreateInfo)
{
    return rad::Ref<VulkanBuffer>{
        new VulkanBuffer(rad::Ref<VulkanDevice>{this}, createInfo, allocationCreateInfo)};
}

rad::Ref<VulkanBuffer> VulkanDevice::CreateBuffer(
    vk::DeviceSize size, vk::BufferUsageFlags usage, VmaMemoryUsage memoryUsage,
    VmaAllocationCreateFlags allocationFlags)
{
    return VulkanBuffer::Create(rad::Ref<VulkanDevice>{this}, size, usage, memoryUsage,
                                allocationFlags);
}

rad::Ref<VulkanBuffer> VulkanDevice::CreateUniformBuffer(
    vk::DeviceSize size, vk::BufferUsageFlags additionalUsage)
{
    vk::BufferCreateInfo bufferInfo;
    bufferInfo.size = size;
    bufferInfo.usage = vk::BufferUsageFlagBits::eUniformBuffer | additionalUsage;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;

    VmaAllocationCreateInfo allocationInfo = {};
    allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                           VMA_ALLOCATION_CREATE_MAPPED_BIT;
    allocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
    allocationInfo.requiredFlags =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    return CreateBuffer(bufferInfo, allocationInfo);
}

rad::Ref<VulkanBuffer> VulkanDevice::CreateUploadBuffer(
    vk::DeviceSize size, vk::BufferUsageFlags additionalUsage)
{
    vk::BufferCreateInfo bufferInfo;
    bufferInfo.size = size;
    bufferInfo.usage = vk::BufferUsageFlagBits::eTransferSrc | additionalUsage;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;

    VmaAllocationCreateInfo allocationInfo = {};
    allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    allocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
    allocationInfo.requiredFlags =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    return CreateBuffer(bufferInfo, allocationInfo);
}

rad::Ref<VulkanBuffer> VulkanDevice::CreateVertexBuffer(
    vk::DeviceSize size, vk::BufferUsageFlags additionalUsage)
{
    return CreateBuffer(size, vk::BufferUsageFlagBits::eVertexBuffer |
                                  vk::BufferUsageFlagBits::eTransferDst |
                                  additionalUsage,
                        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
}

rad::Ref<VulkanBuffer> VulkanDevice::CreateIndexBuffer(
    vk::DeviceSize size, vk::BufferUsageFlags additionalUsage)
{
    return CreateBuffer(size, vk::BufferUsageFlagBits::eIndexBuffer |
                                  vk::BufferUsageFlagBits::eTransferDst |
                                  additionalUsage,
                        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
}

rad::Ref<VulkanBuffer> VulkanDevice::CreateStorageBuffer(
    vk::DeviceSize size, vk::BufferUsageFlags additionalUsage)
{
    return CreateBuffer(size, vk::BufferUsageFlagBits::eStorageBuffer |
                                  vk::BufferUsageFlagBits::eTransferSrc |
                                  vk::BufferUsageFlagBits::eTransferDst |
                                  additionalUsage,
                        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
}

rad::Ref<VulkanImage> VulkanDevice::CreateImage(
    const vk::ImageCreateInfo& createInfo,
    const VmaAllocationCreateInfo& allocationCreateInfo)
{
    return VulkanImage::Create(rad::Ref<VulkanDevice>{this}, createInfo,
                               allocationCreateInfo);
}

rad::Ref<VulkanImage> VulkanDevice::CreateImage2D(
    uint32_t width, uint32_t height, vk::Format format,
    vk::ImageUsageFlags usage, VmaMemoryUsage memoryUsage,
    VmaAllocationCreateFlags allocationFlags, uint32_t mipLevels,
    uint32_t arrayLayers, vk::SampleCountFlagBits samples, vk::ImageTiling tiling)
{
    vk::ImageCreateInfo createInfo;
    createInfo.imageType = vk::ImageType::e2D;
    createInfo.format = format;
    createInfo.extent = vk::Extent3D{width, height, 1};
    createInfo.mipLevels = mipLevels;
    createInfo.arrayLayers = arrayLayers;
    createInfo.samples = samples;
    createInfo.tiling = tiling;
    createInfo.usage = usage;
    createInfo.sharingMode = vk::SharingMode::eExclusive;
    createInfo.initialLayout = vk::ImageLayout::eUndefined;

    VmaAllocationCreateInfo allocationCreateInfo = {};
    allocationCreateInfo.flags = allocationFlags;
    allocationCreateInfo.usage = memoryUsage;
    return CreateImage(createInfo, allocationCreateInfo);
}

rad::Ref<VulkanSampler> VulkanDevice::CreateSampler(
    const vk::SamplerCreateInfo& createInfo)
{
    return rad::Ref<VulkanSampler>{
        new VulkanSampler(rad::Ref<VulkanDevice>{this}, createInfo)};
}

rad::Ref<VulkanSampler> VulkanDevice::CreateSampler(
    vk::Filter filter, vk::SamplerAddressMode addressMode,
    vk::SamplerMipmapMode mipmapMode, float maxLod)
{
    vk::SamplerCreateInfo createInfo;
    createInfo.magFilter = filter;
    createInfo.minFilter = filter;
    createInfo.mipmapMode = mipmapMode;
    createInfo.addressModeU = addressMode;
    createInfo.addressModeV = addressMode;
    createInfo.addressModeW = addressMode;
    createInfo.maxLod = maxLod;
    return CreateSampler(createInfo);
}

rad::Ref<VulkanFence> VulkanDevice::CreateFence(
    const vk::FenceCreateInfo& createInfo)
{
    return rad::Ref<VulkanFence>{
        new VulkanFence(rad::Ref<VulkanDevice>{this}, createInfo)};
}

rad::Ref<VulkanFence> VulkanDevice::CreateFence(bool signaled)
{
    vk::FenceCreateInfo createInfo;
    if (signaled)
    {
        createInfo.flags = vk::FenceCreateFlagBits::eSignaled;
    }
    return CreateFence(createInfo);
}

rad::Ref<VulkanSemaphore> VulkanDevice::CreateSemaphore(
    const vk::SemaphoreCreateInfo& createInfo)
{
    return rad::Ref<VulkanSemaphore>{
        new VulkanSemaphore(rad::Ref<VulkanDevice>{this}, createInfo)};
}

rad::Ref<VulkanSemaphore> VulkanDevice::CreateSemaphore()
{
    return CreateSemaphore(vk::SemaphoreCreateInfo{});
}

rad::Ref<VulkanEvent> VulkanDevice::CreateEvent(
    const vk::EventCreateInfo& createInfo)
{
    return rad::Ref<VulkanEvent>{
        new VulkanEvent(rad::Ref<VulkanDevice>{this}, createInfo)};
}

rad::Ref<VulkanEvent> VulkanDevice::CreateEvent()
{
    return CreateEvent(vk::EventCreateInfo{});
}

rad::Ref<VulkanCommandPool> VulkanDevice::CreateCommandPool(
    VulkanQueueFamily queueFamily, vk::CommandPoolCreateFlags flags)
{
    return rad::Ref<VulkanCommandPool>{
        new VulkanCommandPool(rad::Ref<VulkanDevice>{this}, queueFamily, flags)};
}

rad::Ref<VulkanUploader> VulkanDevice::CreateUploader()
{
    return rad::Ref<VulkanUploader>{
        new VulkanUploader(rad::Ref<VulkanDevice>{this})};
}

rad::Ref<VulkanQueryPool> VulkanDevice::CreateQueryPool(
    const vk::QueryPoolCreateInfo& createInfo)
{
    return rad::Ref<VulkanQueryPool>{
        new VulkanQueryPool(rad::Ref<VulkanDevice>{this}, createInfo)};
}

rad::Ref<VulkanQueryPool> VulkanDevice::CreateQueryPool(
    vk::QueryType queryType, uint32_t queryCount,
    vk::QueryPipelineStatisticFlags pipelineStatistics,
    vk::QueryPoolCreateFlags flags)
{
    vk::QueryPoolCreateInfo createInfo;
    createInfo.flags = flags;
    createInfo.queryType = queryType;
    createInfo.queryCount = queryCount;
    createInfo.pipelineStatistics = pipelineStatistics;
    return CreateQueryPool(createInfo);
}

rad::Ref<VulkanRenderPass> VulkanDevice::CreateRenderPass(
    const vk::RenderPassCreateInfo& createInfo)
{
    return rad::Ref<VulkanRenderPass>{
        new VulkanRenderPass(rad::Ref<VulkanDevice>{this}, createInfo)};
}

rad::Ref<VulkanRenderPass> VulkanDevice::CreateRenderPass(
    vk::ArrayProxy<const vk::AttachmentDescription> attachments,
    vk::ArrayProxy<const vk::SubpassDescription> subpasses,
    vk::ArrayProxy<const vk::SubpassDependency> dependencies,
    vk::RenderPassCreateFlags flags)
{
    vk::RenderPassCreateInfo createInfo;
    createInfo.flags = flags;
    createInfo.attachmentCount = attachments.size();
    createInfo.pAttachments = attachments.data();
    createInfo.subpassCount = subpasses.size();
    createInfo.pSubpasses = subpasses.data();
    createInfo.dependencyCount = dependencies.size();
    createInfo.pDependencies = dependencies.data();
    return CreateRenderPass(createInfo);
}

rad::Ref<VulkanFramebuffer> VulkanDevice::CreateFramebuffer(
    const VulkanFramebufferCreateInfo& createInfo)
{
    return rad::Ref<VulkanFramebuffer>{
        new VulkanFramebuffer(rad::Ref<VulkanDevice>{this}, createInfo)};
}

rad::Ref<VulkanFramebuffer> VulkanDevice::CreateFramebuffer(
    rad::Ref<VulkanRenderPass> renderPass,
    std::vector<rad::Ref<VulkanImageView>> attachments, uint32_t width,
    uint32_t height, uint32_t layers)
{
    VulkanFramebufferCreateInfo createInfo;
    createInfo.m_renderPass = std::move(renderPass);
    createInfo.m_attachments = std::move(attachments);
    createInfo.m_width = width;
    createInfo.m_height = height;
    createInfo.m_layers = layers;
    return CreateFramebuffer(createInfo);
}

rad::Ref<VulkanShaderModule> VulkanDevice::CreateShaderModule(
    const vk::ShaderModuleCreateInfo& createInfo)
{
    return rad::Ref<VulkanShaderModule>{
        new VulkanShaderModule(rad::Ref<VulkanDevice>{this}, createInfo)};
}

rad::Ref<VulkanShaderModule> VulkanDevice::CreateShaderModule(
    std::span<const uint32_t> spirv)
{
    if (spirv.empty())
    {
        throw std::invalid_argument("Shader module SPIR-V must not be empty");
    }

    vk::ShaderModuleCreateInfo createInfo;
    createInfo.codeSize = spirv.size_bytes();
    createInfo.pCode = spirv.data();
    return CreateShaderModule(createInfo);
}

rad::Ref<VulkanPipelineLayout> VulkanDevice::CreatePipelineLayout(
    const vk::PipelineLayoutCreateInfo& createInfo)
{
    return rad::Ref<VulkanPipelineLayout>{
        new VulkanPipelineLayout(rad::Ref<VulkanDevice>{this}, createInfo)};
}

rad::Ref<VulkanPipelineLayout> VulkanDevice::CreatePipelineLayout(
    vk::ArrayProxy<const vk::DescriptorSetLayout> setLayouts,
    vk::ArrayProxy<const vk::PushConstantRange> pushConstantRanges,
    vk::PipelineLayoutCreateFlags flags)
{
    vk::PipelineLayoutCreateInfo createInfo;
    createInfo.flags = flags;
    createInfo.setLayoutCount = setLayouts.size();
    createInfo.pSetLayouts = setLayouts.data();
    createInfo.pushConstantRangeCount = pushConstantRanges.size();
    createInfo.pPushConstantRanges = pushConstantRanges.data();
    return CreatePipelineLayout(createInfo);
}

rad::Ref<VulkanPipelineLayout> VulkanDevice::CreatePipelineLayout(
    std::span<const rad::Ref<VulkanDescriptorSetLayout>> setLayouts,
    vk::ArrayProxy<const vk::PushConstantRange> pushConstantRanges,
    vk::PipelineLayoutCreateFlags flags)
{
    std::vector<vk::DescriptorSetLayout> handles;
    handles.reserve(setLayouts.size());
    for (const rad::Ref<VulkanDescriptorSetLayout>& setLayout : setLayouts)
    {
        if (!setLayout)
        {
            throw std::invalid_argument(
                "Pipeline layout set layouts must contain valid layouts");
        }
        if (setLayout->GetDevice() != this)
        {
            throw std::invalid_argument(
                "Pipeline layout set layout belongs to a different VulkanDevice");
        }
        handles.push_back(setLayout->GetHandle());
    }
    return CreatePipelineLayout(handles, pushConstantRanges, flags);
}

rad::Ref<VulkanGraphicsPipeline> VulkanDevice::CreateGraphicsPipeline(
    const VulkanGraphicsPipelineCreateInfo& createInfo)
{
    return rad::Ref<VulkanGraphicsPipeline>{
        new VulkanGraphicsPipeline(rad::Ref<VulkanDevice>{this}, createInfo)};
}

rad::Ref<VulkanComputePipeline> VulkanDevice::CreateComputePipeline(
    const VulkanComputePipelineCreateInfo& createInfo)
{
    return rad::Ref<VulkanComputePipeline>{
        new VulkanComputePipeline(rad::Ref<VulkanDevice>{this}, createInfo)};
}

rad::Ref<VulkanComputePipeline> VulkanDevice::CreateComputePipeline(
    rad::Ref<VulkanShaderModule> shaderModule,
    rad::Ref<VulkanPipelineLayout> layout, std::string_view entryPoint)
{
    VulkanComputePipelineCreateInfo createInfo;
    createInfo.m_stage.m_stage = vk::ShaderStageFlagBits::eCompute;
    createInfo.m_stage.m_module = std::move(shaderModule);
    createInfo.m_stage.m_entryPoint = entryPoint;
    createInfo.m_layout = std::move(layout);
    return CreateComputePipeline(createInfo);
}

rad::Ref<VulkanDescriptorSetLayout> VulkanDevice::CreateDescriptorSetLayout(
    const vk::DescriptorSetLayoutCreateInfo& createInfo)
{
    return rad::Ref<VulkanDescriptorSetLayout>{
        new VulkanDescriptorSetLayout(rad::Ref<VulkanDevice>{this}, createInfo)};
}

rad::Ref<VulkanDescriptorSetLayout> VulkanDevice::CreateDescriptorSetLayout(
    vk::ArrayProxy<const vk::DescriptorSetLayoutBinding> bindings,
    vk::DescriptorSetLayoutCreateFlags flags)
{
    vk::DescriptorSetLayoutCreateInfo createInfo;
    createInfo.flags = flags;
    createInfo.bindingCount = bindings.size();
    createInfo.pBindings = bindings.data();
    return CreateDescriptorSetLayout(createInfo);
}

rad::Ref<VulkanDescriptorPool> VulkanDevice::CreateDescriptorPool(
    const vk::DescriptorPoolCreateInfo& createInfo)
{
    return rad::Ref<VulkanDescriptorPool>{
        new VulkanDescriptorPool(rad::Ref<VulkanDevice>{this}, createInfo)};
}

rad::Ref<VulkanDescriptorPool> VulkanDevice::CreateDescriptorPool(
    uint32_t maxSets, vk::ArrayProxy<const vk::DescriptorPoolSize> poolSizes,
    vk::DescriptorPoolCreateFlags flags)
{
    vk::DescriptorPoolCreateInfo createInfo;
    createInfo.flags = flags;
    createInfo.maxSets = maxSets;
    createInfo.poolSizeCount = poolSizes.size();
    createInfo.pPoolSizes = poolSizes.data();
    return CreateDescriptorPool(createInfo);
}

void VulkanDevice::QueryPropertiesAndFeatures()
{
    const auto& dispatcher = m_instance->GetDispatcher();
    m_properties = m_physicalDevice.getProperties(dispatcher);
    m_features = m_physicalDevice.getFeatures(dispatcher);
    m_queueFamilyProperties = m_physicalDevice.getQueueFamilyProperties(dispatcher);
    m_memoryProperties = m_physicalDevice.getMemoryProperties(dispatcher);

    m_apiVersion = std::min(m_properties.apiVersion, uint32_t(TargetVulkanApiVersion));
    if (!m_apiVersion.IsGreaterEqualThan(VK_API_VERSION_1_1))
    {
        m_properties2.properties = m_properties;
        m_features2.features = m_features;
        return;
    }

    VulkanStructureChain propertiesChain{m_properties2};
    propertiesChain.Link(m_vk11Properties);
    if (m_apiVersion.IsGreaterEqualThan(VK_API_VERSION_1_2))
    {
        propertiesChain.Link(m_driverProperties);
        propertiesChain.Link(m_vk12Properties);
    }
    if (m_apiVersion.IsGreaterEqualThan(VK_API_VERSION_1_3))
    {
        propertiesChain.Link(m_vk13Properties);
    }
    m_physicalDevice.getProperties2(&propertiesChain, dispatcher);
    m_properties = m_properties2.properties;

    VulkanStructureChain featuresChain{m_features2};
    featuresChain.Link(m_Vulkan11Features);
    if (m_apiVersion.IsGreaterEqualThan(VK_API_VERSION_1_2))
    {
        featuresChain.Link(m_Vulkan12Features);
    }
    if (m_apiVersion.IsGreaterEqualThan(VK_API_VERSION_1_3))
    {
        featuresChain.Link(m_Vulkan13Features);
    }
    if (m_apiVersion.IsGreaterEqualThan(VK_API_VERSION_1_4))
    {
        featuresChain.Link(m_Vulkan14Features);
    }
    m_physicalDevice.getFeatures2(&featuresChain, dispatcher);
    m_features = m_features2.features;
}

void VulkanDevice::SelectQueueFamilies(vk::SurfaceKHR presentSurface)
{
    for (uint32_t index = 0; index < m_queueFamilyProperties.size(); ++index)
    {
        const vk::QueueFamilyProperties& properties = m_queueFamilyProperties[index];
        SD_LOG(info, "Queue family #{}: flags={}", index, vk::to_string(properties.queueFlags));
    }

    const auto findQueueFamily = [this](vk::QueueFlags required, vk::QueueFlags excluded)
        -> uint32_t
    {
        for (uint32_t index = 0; index < m_queueFamilyProperties.size(); ++index)
        {
            const vk::QueueFamilyProperties& properties = m_queueFamilyProperties[index];
            if (properties.queueCount > 0 && HasAllBits(properties.queueFlags, required) &&
                HasNoBits(properties.queueFlags, excluded))
            {
                return index;
            }
        }
        return VK_QUEUE_FAMILY_IGNORED;
    };

    uint32_t graphicsQueueFamilyIndex =
        findQueueFamily(vk::QueueFlagBits::eGraphics, vk::QueueFlags{});

    // Prefer a graphics family that can present (matches vkcube's combined-queue path).
    if (presentSurface && graphicsQueueFamilyIndex != VK_QUEUE_FAMILY_IGNORED)
    {
        uint32_t combinedQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        for (uint32_t index = 0; index < m_queueFamilyProperties.size(); ++index)
        {
            const vk::QueueFamilyProperties& properties = m_queueFamilyProperties[index];
            if ((properties.queueCount > 0) &&
                HasAllBits(properties.queueFlags, vk::QueueFlagBits::eGraphics) &&
                GetSurfaceSupport(presentSurface, index))
            {
                combinedQueueFamilyIndex = index;
                break;
            }
        }

        if (combinedQueueFamilyIndex != VK_QUEUE_FAMILY_IGNORED)
        {
            graphicsQueueFamilyIndex = combinedQueueFamilyIndex;
        }
        else
        {
            SD_LOG(warn,
                   "No graphics queue family can present to the given surface; "
                   "using graphics family #{}",
                   graphicsQueueFamilyIndex);
        }
    }

    m_queueFamilyIndices[rad::UnderlyingCast(VulkanQueueFamily::Graphics)] =
        graphicsQueueFamilyIndex;

    uint32_t computeQueueFamilyIndex =
        findQueueFamily(vk::QueueFlagBits::eCompute, vk::QueueFlagBits::eGraphics);
    if (computeQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED)
    {
        computeQueueFamilyIndex =
            findQueueFamily(vk::QueueFlagBits::eCompute, vk::QueueFlags{});
    }
    m_queueFamilyIndices[rad::UnderlyingCast(VulkanQueueFamily::Compute)] =
        computeQueueFamilyIndex;

    uint32_t transferQueueFamilyIndex =
        findQueueFamily(vk::QueueFlagBits::eTransfer,
                        vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute);
    if (transferQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED)
    {
        transferQueueFamilyIndex =
            findQueueFamily(vk::QueueFlagBits::eTransfer, vk::QueueFlags{});
    }
    m_queueFamilyIndices[rad::UnderlyingCast(VulkanQueueFamily::Transfer)] =
        transferQueueFamilyIndex;

    if (graphicsQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED &&
        computeQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED)
    {
        throw std::runtime_error(
            "Physical device has no graphics- or compute-capable queue family");
    }

    const auto queueFamilyIndexString = [](uint32_t index)
    {
        return index == VK_QUEUE_FAMILY_IGNORED ? std::string{"N/A"}
                                                : "#" + std::to_string(index);
    };
    SD_LOG(info, "Selected queue families: Graphics={}; Compute={}; Transfer={}",
           queueFamilyIndexString(graphicsQueueFamilyIndex),
           queueFamilyIndexString(computeQueueFamilyIndex),
           queueFamilyIndexString(transferQueueFamilyIndex));
}

} // namespace sd
