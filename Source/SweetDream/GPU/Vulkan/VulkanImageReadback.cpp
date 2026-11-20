#include "VulkanImageReadback.h"

#include <SweetDream/GPU/Vulkan/VulkanBuffer.h>
#include <SweetDream/GPU/Vulkan/VulkanCommand.h>
#include <SweetDream/GPU/Vulkan/VulkanDevice.h>
#include <SweetDream/GPU/Vulkan/VulkanImage.h>

#include <rad/IO/Image.h>

#include <limits>
#include <stdexcept>

namespace sd
{
namespace
{

constexpr auto HdrFormat = vk::Format::eR32G32B32A32Sfloat;

std::size_t GetTexelSize(vk::Format format)
{
    switch (format)
    {
    case vk::Format::eR8G8B8A8Unorm:
    case vk::Format::eR8G8B8A8Srgb:
    case vk::Format::eB8G8R8A8Unorm:
    case vk::Format::eB8G8R8A8Srgb:
        return 4;
    case HdrFormat:
        return 16;
    default:
        throw std::invalid_argument("Unsupported VulkanImageReadback format: " + vk::to_string(format));
    }
}

} // namespace

VulkanImageReadback::VulkanImageReadback(VulkanDevice* device) : m_device(device)
{
    if (!m_device)
    {
        throw std::invalid_argument("VulkanImageReadback requires a device");
    }
}

VulkanImageReadback::~VulkanImageReadback() = default;

VulkanImageReadback::Layout VulkanImageReadback::Execute(
    VulkanCommandBuffer* command, VulkanImage* source, VulkanImageState before,
    VulkanImageState after)
{
    return ExecuteImpl(command, source, before, after, false);
}

VulkanImageReadback::Layout VulkanImageReadback::ExecuteHDR(
    VulkanCommandBuffer* command, VulkanImage* source, VulkanImageState before,
    VulkanImageState after)
{
    return ExecuteImpl(command, source, before, after, true);
}

VulkanImageReadback::Layout VulkanImageReadback::ExecuteImpl(
    VulkanCommandBuffer* command, VulkanImage* source, VulkanImageState before,
    VulkanImageState after, bool hdr)
{
    if (!command || !source || command->GetDevice() != m_device.get() ||
        source->GetDevice() != m_device.get())
    {
        throw std::invalid_argument(
            "VulkanImageReadback requires an image and a command buffer on its device");
    }
    if (source->GetImageType() != vk::ImageType::e2D ||
        source->GetSamples() != vk::SampleCountFlagBits::e1 ||
        !(source->GetUsage() & vk::ImageUsageFlagBits::eTransferSrc))
    {
        throw std::invalid_argument(
            "VulkanImageReadback requires a single-sample 2D image with transfer-source usage");
    }
    if (before.layout == vk::ImageLayout::eUndefined || after.layout == vk::ImageLayout::eUndefined ||
        after.layout == vk::ImageLayout::ePreinitialized)
    {
        throw std::invalid_argument(
            "VulkanImageReadback requires initialized source data and a valid final layout");
    }
    const auto queueFlags =
        m_device->GetQueueFamilyProperties(command->GetCommandPool()->GetQueueFamily()).queueFlags;
    if (!(queueFlags & (vk::QueueFlagBits::eTransfer | vk::QueueFlagBits::eGraphics |
                        vk::QueueFlagBits::eCompute)))
    {
        throw std::invalid_argument("VulkanImageReadback requires a transfer-capable queue");
    }

    const auto sourceFormat = source->GetFormat();
    const bool blit = hdr && sourceFormat != HdrFormat;
    if (blit && sourceFormat != vk::Format::eR16G16B16A16Sfloat &&
        sourceFormat != vk::Format::eB10G11R11UfloatPack32)
    {
        throw std::invalid_argument("VulkanImageReadback::ExecuteHDR does not support " +
                                    vk::to_string(sourceFormat));
    }

    const auto format = hdr ? HdrFormat : sourceFormat;
    const auto texelSize = GetTexelSize(format);
    const auto extent = source->GetExtent();
    constexpr auto maxSize = std::numeric_limits<std::size_t>::max();
    if (extent.width == 0 || extent.height == 0 ||
        extent.width > maxSize / texelSize / extent.height)
    {
        throw std::length_error("VulkanImageReadback image dimensions are invalid or too large");
    }
    const auto rowBytes = extent.width * texelSize;
    const auto sizeBytes = rowBytes * extent.height;
    if (blit)
    {
        if (!(queueFlags & vk::QueueFlagBits::eGraphics))
        {
            throw std::invalid_argument("VulkanImageReadback HDR blit requires a graphics queue");
        }
        const auto tiling = source->GetTiling();
        if (tiling != vk::ImageTiling::eOptimal && tiling != vk::ImageTiling::eLinear)
        {
            throw std::invalid_argument("VulkanImageReadback HDR blit requires optimal or linear tiling");
        }
        const auto physicalDevice = m_device->GetPhysicalDevice();
        const auto& dispatcher = m_device->GetDispatcher();
        const auto properties = physicalDevice.getFormatProperties(sourceFormat, dispatcher);
        const auto sourceFeatures = tiling == vk::ImageTiling::eOptimal
                                        ? properties.optimalTilingFeatures
                                        : properties.linearTilingFeatures;
        const auto targetFeatures = physicalDevice.getFormatProperties(HdrFormat, dispatcher)
                                        .optimalTilingFeatures;
        constexpr auto required = vk::FormatFeatureFlagBits::eBlitDst |
                                  vk::FormatFeatureFlagBits::eTransferSrc;
        if (!(sourceFeatures & vk::FormatFeatureFlagBits::eBlitSrc) ||
            !HasAllBits(targetFeatures, required))
        {
            throw std::runtime_error("VulkanImageReadback cannot blit " + vk::to_string(sourceFormat) +
                                     " to RGBA32F on this device");
        }
        constexpr auto maxOffset = static_cast<uint32_t>(std::numeric_limits<int32_t>::max());
        if (extent.width > maxOffset || extent.height > maxOffset)
        {
            throw std::length_error("VulkanImageReadback HDR blit extent exceeds the offset range");
        }
        if (!m_hdrImage || m_hdrImage->GetExtent() != extent)
        {
            m_hdrImage = m_device->CreateImage2D(extent.width, extent.height, HdrFormat,
                                                 vk::ImageUsageFlagBits::eTransferDst |
                                                 vk::ImageUsageFlagBits::eTransferSrc);
        }
    }
    if (!m_buffer || m_buffer->GetSize() < sizeBytes)
    {
        m_buffer = m_device->CreateBuffer(sizeBytes, vk::BufferUsageFlagBits::eTransferDst,
                                          VMA_MEMORY_USAGE_AUTO,
                                          VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
    }

    constexpr vk::ImageSubresourceRange range{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    constexpr vk::ImageSubresourceLayers layers{vk::ImageAspectFlagBits::eColor, 0, 0, 1};
    constexpr auto transferLayout = vk::ImageLayout::eTransferSrcOptimal;
    constexpr auto blitStage = vk::PipelineStageFlagBits2::eBlit;
    constexpr auto copyStage = vk::PipelineStageFlagBits2::eCopy;
    constexpr auto transferRead = vk::AccessFlagBits2::eTransferRead;
    constexpr auto transferWrite = vk::AccessFlagBits2::eTransferWrite;
    const auto sourceReadStage = blit ? blitStage : copyStage;
    const auto image = source->GetHandle();
    const auto buffer = m_buffer->GetHandle();
    command->ImageMemoryBarrier(image, range, before.layout, transferLayout,
                                before.stage, before.access, sourceReadStage, transferRead);

    auto copyImage = image;
    if (blit)
    {
        copyImage = m_hdrImage->GetHandle();
        constexpr auto dstLayout = vk::ImageLayout::eTransferDstOptimal;
        command->ImageMemoryBarrier(copyImage, range, vk::ImageLayout::eUndefined, dstLayout,
                                    {}, {}, blitStage, transferWrite);
        vk::ImageBlit region;
        region.srcSubresource = layers;
        region.dstSubresource = layers;
        region.srcOffsets[1] = vk::Offset3D{static_cast<int32_t>(extent.width),
                                           static_cast<int32_t>(extent.height), 1};
        region.dstOffsets[1] = region.srcOffsets[1];
        command->BlitImage(image, transferLayout, copyImage, dstLayout, region, vk::Filter::eNearest);
        command->ImageMemoryBarrier(copyImage, range, dstLayout, transferLayout,
                                    blitStage, transferWrite, copyStage, transferRead);
    }

    vk::BufferImageCopy copy;
    copy.imageSubresource = layers;
    copy.imageExtent = extent;
    command->CopyImageToBuffer(copyImage, transferLayout, buffer, copy);
    command->BufferMemoryBarrier(buffer, copyStage, transferWrite,
                                 vk::PipelineStageFlagBits2::eHost, vk::AccessFlagBits2::eHostRead,
                                 0, sizeBytes);
    command->ImageMemoryBarrier(image, range, transferLayout, after.layout,
                                sourceReadStage | before.stage, transferRead | before.access,
                                after.stage, after.access);

    m_layout = {format, {extent.width, extent.height}, rowBytes, sizeBytes};
    return m_layout;
}

void VulkanImageReadback::ReadHost(void* destination, std::size_t capacityBytes)
{
    RequireRecordedCopy();
    if (!destination)
    {
        throw std::invalid_argument("VulkanImageReadback::ReadHost requires a destination");
    }
    if (capacityBytes < m_layout.sizeBytes)
    {
        throw std::out_of_range("VulkanImageReadback::ReadHost destination is too small");
    }
    m_buffer->ReadHost(destination, m_layout.sizeBytes);
}

bool VulkanImageReadback::SavePNG(const std::string& fileName)
{
    RequireRecordedCopy();
    const auto format = m_layout.format;
    const bool bgra = format == vk::Format::eB8G8R8A8Unorm || format == vk::Format::eB8G8R8A8Srgb;
    if (!bgra && format != vk::Format::eR8G8B8A8Unorm && format != vk::Format::eR8G8B8A8Srgb)
    {
        throw std::invalid_argument("VulkanImageReadback::SavePNG requires RGBA8 or BGRA8");
    }

    const auto [width, height] = m_layout.extent;
    // The PNG encoder uses int-sized offsets and adds a filter byte to each row.
    constexpr auto maxInt = static_cast<std::size_t>(std::numeric_limits<int>::max());
    if ((m_layout.rowBytes + 1) > maxInt / height)
    {
        return false;
    }
    rad::ImageUnorm8 pixels(static_cast<int>(width), static_cast<int>(height), 4);
    ReadHost(pixels.Data(), m_layout.sizeBytes);
    if (bgra)
    {
        auto* data = pixels.Data();
        for (std::size_t offset = 0; offset < m_layout.sizeBytes; offset += 4)
        {
            std::swap(data[offset], data[offset + 2]);
        }
    }
    return pixels.SavePNG(fileName);
}

bool VulkanImageReadback::SaveHDR(const std::string& fileName)
{
    RequireRecordedCopy();
    if (m_layout.format != HdrFormat)
    {
        throw std::invalid_argument("VulkanImageReadback::SaveHDR requires RGBA32F");
    }

    const auto [width, height] = m_layout.extent;
    constexpr auto maxInt = static_cast<std::size_t>(std::numeric_limits<int>::max());
    if (width > maxInt / (3 * sizeof(float)) || width > maxInt / 3 / height)
    {
        return false;
    }
    std::vector<float> pixels(m_layout.sizeBytes / sizeof(float));
    ReadHost(pixels.data(), pixels.size() * sizeof(float));
    // The RGBE encoder takes RGB only. Drop alpha in place.
    const auto pixelCount = pixels.size() / 4;
    for (std::size_t i = 0; i < pixelCount; ++i)
    {
        pixels[3 * i]     = pixels[4 * i];
        pixels[3 * i + 1] = pixels[4 * i + 1];
        pixels[3 * i + 2] = pixels[4 * i + 2];
    }
    pixels.resize(pixelCount * 3);
    return rad::ImageFloat32(static_cast<int>(width), static_cast<int>(height), 3,
                             std::move(pixels)).SaveHDR(fileName);
}

void VulkanImageReadback::RequireRecordedCopy() const
{
    if (m_layout.sizeBytes == 0)
    {
        throw std::logic_error("VulkanImageReadback requires a recorded copy");
    }
}

} // namespace sd
