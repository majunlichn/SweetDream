#include "VulkanMipmap.h"

#include <SweetDream/GPU/Vulkan/VulkanCommand.h>
#include <SweetDream/GPU/Vulkan/VulkanDescriptor.h>
#include <SweetDream/GPU/Vulkan/VulkanImage.h>
#include <SweetDream/GPU/Vulkan/VulkanPipeline.h>

#include <algorithm>
#include <array>
#include <bit>
#include <stdexcept>
#include <utility>

namespace sd
{
namespace
{

constexpr uint32_t GeneralRgba8[] =
#include "VulkanMipmapGeneralRGBA8.inc"
    ;
constexpr uint32_t GeneralRgba32f[] =
#include "VulkanMipmapGeneralRGBA32F.inc"
    ;
constexpr uint32_t FastRgba8[] =
#include "VulkanMipmapFastRGBA8.inc"
    ;
constexpr uint32_t FastRgba32f[] =
#include "VulkanMipmapFastRGBA32F.inc"
    ;

constexpr auto ComputeStage = vk::PipelineStageFlagBits2::eComputeShader;
constexpr auto StorageRead = vk::AccessFlagBits2::eShaderStorageRead;
constexpr auto StorageWrite = vk::AccessFlagBits2::eShaderStorageWrite;
constexpr auto General = vk::ImageLayout::eGeneral;
constexpr auto StorageImage = vk::DescriptorType::eStorageImage;
constexpr uint32_t MaxGeneralLevels = 2;
constexpr uint32_t MaxFastLevels = 6;
constexpr uint32_t FastSubgroupSize = 32;
constexpr uint32_t FastWorkgroupSize = 256;

bool SupportsFastPipeline(const VulkanDevice& device)
{
    const auto& limits = device.GetProperties().limits;
    const auto& vulkan11 = device.GetVulkan11Properties();
    const auto& vulkan13 = device.GetVulkan13Properties();
    const auto& features = device.GetVulkan13Features();
    constexpr auto compute = vk::ShaderStageFlagBits::eCompute;
    return device.GetApiVersion().IsGreaterEqualThan(VK_API_VERSION_1_3) &&
           features.subgroupSizeControl && features.computeFullSubgroups &&
           (vulkan11.subgroupSupportedStages & compute) &&
           (vulkan11.subgroupSupportedOperations & vk::SubgroupFeatureFlagBits::eShuffle) &&
           (vulkan13.requiredSubgroupSizeStages & compute) &&
           vulkan13.minSubgroupSize <= FastSubgroupSize && vulkan13.maxSubgroupSize >= FastSubgroupSize &&
           vulkan13.maxComputeWorkgroupSubgroups >= FastWorkgroupSize / FastSubgroupSize &&
           limits.maxComputeWorkGroupInvocations >= FastWorkgroupSize &&
           limits.maxComputeWorkGroupSize[0] >= FastWorkgroupSize &&
           limits.maxPerStageDescriptorStorageImages >= 1 + MaxFastLevels &&
           limits.maxDescriptorSetStorageImages >= 1 + MaxFastLevels;
}

std::span<const uint32_t> MipmapShader(vk::Format format, bool fast)
{
    if (format == vk::Format::eR8G8B8A8Unorm)
    {
        return fast ? std::span<const uint32_t>{FastRgba8} : std::span<const uint32_t>{GeneralRgba8};
    }
    return fast ? std::span<const uint32_t>{FastRgba32f} : std::span<const uint32_t>{GeneralRgba32f};
}

vk::Extent2D LevelExtent(const VulkanImage& image, uint32_t level)
{
    return {std::max(1u, image.GetWidth() >> level), std::max(1u, image.GetHeight() >> level)};
}

uint32_t DivideRoundUp(uint32_t value, uint32_t divisor)
{
    return value / divisor + (value % divisor != 0);
}

} // namespace

VulkanMipmap::VulkanMipmap(VulkanDevice* device) : m_device(device)
{
    if (!device)
    {
        throw std::invalid_argument("VulkanMipmap requires a device");
    }
}

VulkanMipmap::~VulkanMipmap() = default;

uint32_t VulkanMipmap::GetMipLevelCount(vk::Extent2D extent) noexcept
{
    return std::bit_width(std::max(extent.width, extent.height));
}

void VulkanMipmap::SetImage(VulkanImage* image)
{
    if (!image || image->GetDevice() != m_device.get() ||
        image->GetImageType() != vk::ImageType::e2D || image->GetArrayLayers() != 1 ||
        image->GetSamples() != vk::SampleCountFlagBits::e1 ||
        image->GetTiling() != vk::ImageTiling::eOptimal ||
        (image->GetFormat() != vk::Format::eR8G8B8A8Unorm &&
         image->GetFormat() != vk::Format::eR32G32B32A32Sfloat))
    {
        throw std::invalid_argument("VulkanMipmap requires an optimal single-layer, single-sample "
                                    "2D RGBA8 UNORM or RGBA32F image on its device");
    }
    if (image == m_image.get())
    {
        return;
    }

    std::vector<rad::Ref<VulkanImageView>> views;
    std::vector<Dispatch> dispatches;
    const uint32_t levels = image->GetMipLevels();
    if (levels > 1)
    {
        const auto features = m_device->GetPhysicalDevice()
                                  .getFormatProperties(image->GetFormat(), m_device->GetDispatcher())
                                  .optimalTilingFeatures;
        if (!(image->GetUsage() & vk::ImageUsageFlagBits::eStorage) ||
            !(features & vk::FormatFeatureFlagBits::eStorageImage))
        {
            throw std::invalid_argument("VulkanMipmap requires storage image support and usage");
        }

        views.reserve(levels);
        for (uint32_t level = 0; level < levels; ++level)
        {
            views.push_back(image->CreateView2D(level));
        }

        const auto& limits = m_device->GetProperties().limits;
        const bool supportsFast = SupportsFastPipeline(*m_device);
        const auto format = image->GetFormat();
        auto generalPipeline = m_generalPipeline;
        auto fastPipeline = m_fastPipeline;
        const auto preparePipeline = [&](Pipeline& variant, bool fast)
        {
            constexpr auto stage = vk::ShaderStageFlagBits::eCompute;
            if (!variant.setLayout)
            {
                const std::array bindings{
                    MakeDescriptorSetLayoutBinding(0, StorageImage, stage),
                    MakeDescriptorSetLayoutBinding(1, StorageImage, stage,
                                                   fast ? MaxFastLevels : MaxGeneralLevels),
                };
                variant.setLayout = m_device->CreateDescriptorSetLayout(bindings);
                variant.layout = m_device->CreatePipelineLayout(
                    variant.setLayout->GetHandle(), vk::PushConstantRange{stage, 0, sizeof(uint32_t)});
            }
            if (variant.format != format)
            {
                VulkanComputePipelineCreateInfo info;
                info.m_layout = variant.layout;
                info.m_stage = VulkanShaderStageInfo{
                    stage, m_device->CreateShaderModule(MipmapShader(format, fast))};
                if (fast)
                {
                    info.m_stage.m_flags = vk::PipelineShaderStageCreateFlagBits::eRequireFullSubgroups;
                    info.m_requiredSubgroupSize = FastSubgroupSize;
                }
                variant.pipeline = m_device->CreateComputePipeline(info);
                variant.format = format;
            }
        };
        std::vector<vk::DescriptorSetLayout> setLayouts;
        uint32_t descriptorCount = 0;
        for (uint32_t src = 0; src + 1 < levels;)
        {
            const auto extent = LevelExtent(*image, src);
            const uint32_t commonDivisibility = static_cast<uint32_t>(
                std::countr_zero(extent.width | extent.height));
            const uint32_t remaining = levels - src - 1;
            // Like nvproPyramidDefaultFastDispatcher, require 4-aligned inputs and
            // enough factors of two on both axes for every fused level.
            if (supportsFast && commonDivisibility >= 2)
            {
                const uint32_t generated = std::min({remaining, commonDivisibility, MaxFastLevels});
                const uint32_t texelsPerGroup = generated == MaxFastLevels ? 4096 : 1024;
                const uint64_t area = uint64_t{extent.width} * extent.height;
                const uint64_t groupsX = (area + texelsPerGroup - 1) / texelsPerGroup;
                if (groupsX <= limits.maxComputeWorkGroupCount[0] && groupsX * FastWorkgroupSize <= (uint64_t{1} << 32))
                {
                    preparePipeline(fastPipeline, true);
                    dispatches.push_back({{}, generated, static_cast<uint32_t>(groupsX), 1, true});
                    setLayouts.push_back(fastPipeline.setLayout->GetHandle());
                    descriptorCount += 1 + MaxFastLevels;
                    src += generated;
                    continue;
                }
            }

            const auto next = LevelExtent(*image, src + 1);
            // Odd intermediate extents can make adjacent nvpro tiles store the same halo texels.
            const auto noHalo = [](uint32_t size) { return size == 1 || size % 2 == 0; };
            const bool two = src + 2 < levels && noHalo(next.width) && noHalo(next.height);
            const auto last = two ? LevelExtent(*image, src + 2) : next;
            const uint32_t x = DivideRoundUp(last.width, two ? 8u : 128u);
            const uint32_t y = two ? DivideRoundUp(last.height, 8u) : last.height;
            if (x > limits.maxComputeWorkGroupCount[0] || y > limits.maxComputeWorkGroupCount[1])
            {
                throw std::invalid_argument("VulkanMipmap dispatch exceeds device workgroup limits");
            }
            preparePipeline(generalPipeline, false);
            dispatches.push_back({{}, two ? 2u : 1u, x, y, false});
            setLayouts.push_back(generalPipeline.setLayout->GetHandle());
            descriptorCount += 1 + MaxGeneralLevels;
            src += dispatches.back().levelCount;
        }
        const auto count = static_cast<uint32_t>(dispatches.size());
        const auto pool = m_device->CreateDescriptorPool(count, vk::DescriptorPoolSize{StorageImage, descriptorCount});
        auto sets = pool->Allocate(setLayouts);
        uint32_t src = 0;
        for (uint32_t i = 0; i < count; ++i)
        {
            auto& dispatch = dispatches[i];
            dispatch.set = std::move(sets[i]);
            const uint32_t capacity = dispatch.fast ? MaxFastLevels : MaxGeneralLevels;
            std::array<vk::DescriptorImageInfo, 1 + MaxFastLevels> infos;
            for (uint32_t level = 0; level <= capacity; ++level)
            {
                // Unused descriptors still need valid views.
                const auto& view = views[src + std::min(level, dispatch.levelCount)];
                infos[level] = {{}, view->GetHandle(), General};
            }
            const std::array writes{
                vk::WriteDescriptorSet{dispatch.set->GetHandle(), 0, 0, 1, StorageImage, infos.data()},
                vk::WriteDescriptorSet{dispatch.set->GetHandle(), 1, 0, capacity, StorageImage, infos.data() + 1},
            };
            m_device->UpdateDescriptorSets(writes);
            src += dispatch.levelCount;
        }

        m_generalPipeline = std::move(generalPipeline);
        m_fastPipeline = std::move(fastPipeline);
    }

    m_dispatches = std::move(dispatches);
    m_views = std::move(views);
    m_image = rad::Ref<VulkanImage>{image};
}

void VulkanMipmap::Execute(VulkanCommandBuffer* command, ImageState baseStateBefore,
                           ImageState lowerStateBefore, ImageState stateAfter) const
{
    if (!m_image || !command || command->GetDevice() != m_device.get())
    {
        throw std::invalid_argument("VulkanMipmap requires an image and a command buffer on its device");
    }
    if (baseStateBefore.layout == vk::ImageLayout::eUndefined ||
        stateAfter.layout == vk::ImageLayout::eUndefined ||
        stateAfter.layout == vk::ImageLayout::ePreinitialized)
    {
        throw std::invalid_argument("VulkanMipmap requires initialized base data and a valid final layout");
    }
    if (!m_dispatches.empty() &&
        !(m_device->GetQueueFamilyProperties(command->GetCommandPool()->GetQueueFamily()).queueFlags &
          vk::QueueFlagBits::eCompute))
    {
        throw std::invalid_argument("VulkanMipmap requires a compute-capable queue");
    }

    const auto transition = [&](uint32_t firstMip, uint32_t count, ImageState from, ImageState to)
    {
        command->ImageMemoryBarrier(
            m_image->GetHandle(), {vk::ImageAspectFlagBits::eColor, firstMip, count, 0, 1},
            from.layout, to.layout, from.stage, from.access, to.stage, to.access);
    };
    if (m_dispatches.empty())
    {
        transition(0, 1, baseStateBefore, stateAfter);
        return;
    }

    const uint32_t outputMipCount = m_image->GetMipLevels() - 1;
    transition(0, 1, baseStateBefore, {General, ComputeStage, StorageRead});
    transition(1, outputMipCount, lowerStateBefore, {General, ComputeStage, StorageWrite});
    vk::Pipeline boundPipeline;
    for (std::size_t i = 0; i < m_dispatches.size(); ++i)
    {
        if (i != 0)
        {
            command->MemoryBarrier(ComputeStage, StorageWrite, ComputeStage, StorageRead);
        }
        const auto& dispatch = m_dispatches[i];
        const auto& variant = dispatch.fast ? m_fastPipeline : m_generalPipeline;
        if (boundPipeline != variant.pipeline->GetHandle())
        {
            boundPipeline = variant.pipeline->GetHandle();
            command->BindPipeline(vk::PipelineBindPoint::eCompute, boundPipeline);
        }
        const auto layout = variant.layout->GetHandle();
        command->BindDescriptorSets(vk::PipelineBindPoint::eCompute, layout, 0, dispatch.set->GetHandle());
        command->PushConstants(layout, vk::ShaderStageFlagBits::eCompute, 0,
                               sizeof(dispatch.levelCount), &dispatch.levelCount);
        command->Dispatch(dispatch.groupsX, dispatch.groupsY, 1);
    }
    transition(0, 1, {General, baseStateBefore.stage | ComputeStage, baseStateBefore.access | StorageRead},
               stateAfter);
    transition(1, outputMipCount, {General, ComputeStage, StorageRead | StorageWrite}, stateAfter);
}

} // namespace sd
