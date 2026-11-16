#include "ShaderToyTexture.h"

#include <SweetDream/GPU/Vulkan/VulkanBuffer.h>
#include <SweetDream/GPU/Vulkan/VulkanFence.h>
#include <SweetDream/GPU/Vulkan/VulkanQueue.h>
#include <SweetDream/Core/IO/Logging.h>

#include <rad/IO/Image.h>
#include <rad/IO/File.h>

#include <algorithm>
#include <bit>
#include <stdexcept>

namespace shadertoy
{
namespace
{

constexpr vk::ImageSubresourceRange BaseLevel{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
constexpr vk::Format TextureFormat = vk::Format::eR8G8B8A8Unorm;

} // namespace

uint32_t GetMipLevelCount(vk::Extent2D extent)
{
    return std::bit_width(std::max(extent.width, extent.height));
}

void CheckMipmapSupport(sd::VulkanDevice* device, vk::Format format)
{
    const auto features = device->GetPhysicalDevice()
                              .getFormatProperties(format, device->GetDispatcher())
                              .optimalTilingFeatures;
    const auto required = vk::FormatFeatureFlagBits::eBlitSrc |
                          vk::FormatFeatureFlagBits::eBlitDst |
                          vk::FormatFeatureFlagBits::eSampledImageFilterLinear |
                          vk::FormatFeatureFlagBits::eTransferSrc;
    if ((features & required) != required)
    {
        throw std::runtime_error("Linear mipmap generation is unsupported for " +
                                 vk::to_string(format));
    }
}

Texture LoadTexture(sd::VulkanDevice* device, const rad::FilePath& path,
                    bool vflip, bool mipmaps)
{
    const auto data = rad::File::ReadAllBytes(path);
    if (!data)
    {
        throw std::runtime_error("Cannot read texture: " + rad::ToUtf8(path) +
                                 "; run download_resources.py from the ShaderToy asset directory");
    }
    auto pixels = rad::ImageUnorm8::LoadFromMemory(data->data(), data->size(), 4);
    if (!pixels)
    {
        throw std::runtime_error("Cannot decode texture: " + rad::ToUtf8(path));
    }
    const vk::Extent2D extent{static_cast<uint32_t>(pixels->Width()),
                              static_cast<uint32_t>(pixels->Height())};
    const uint32_t maximum = device->GetProperties().limits.maxImageDimension2D;
    if (extent.width > maximum || extent.height > maximum)
    {
        throw std::invalid_argument("Texture dimensions exceed Vulkan device limits: " +
                                    rad::ToUtf8(path));
    }
    if (vflip)
    {
        const std::size_t rowBytes = static_cast<std::size_t>(pixels->Width()) * 4;
        for (int y = 0; y < pixels->Height() / 2; ++y)
        {
            auto* row = pixels->Pixel(0, y);
            std::swap_ranges(row, row + rowBytes, pixels->Pixel(0, pixels->Height() - 1 - y));
        }
    }
    const auto features = device->GetPhysicalDevice()
                              .getFormatProperties(TextureFormat, device->GetDispatcher())
                              .optimalTilingFeatures;
    const auto required = vk::FormatFeatureFlagBits::eSampledImage |
                          vk::FormatFeatureFlagBits::eTransferDst;
    if ((features & required) != required)
    {
        throw std::runtime_error("RGBA8 texture upload/sampling is unsupported");
    }
    const uint32_t levels = mipmaps ? GetMipLevelCount(extent) : 1;
    if (levels > 1)
    {
        CheckMipmapSupport(device, TextureFormat);
    }
    auto usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
    if (levels > 1)
    {
        usage |= vk::ImageUsageFlagBits::eTransferSrc;
    }
    Texture texture;
    texture.image = device->CreateImage2D(
        extent.width, extent.height, TextureFormat, usage, VMA_MEMORY_USAGE_AUTO, 0, levels);
    texture.view = texture.image->CreateView2D(0, levels);

    const vk::DeviceSize bytes = static_cast<vk::DeviceSize>(extent.width) * extent.height * 4;
    auto staging = device->CreateUploadBuffer(bytes);
    staging->WriteHost(pixels->Data(), bytes);
    auto pool = device->CreateCommandPool(sd::VulkanQueueFamily::Graphics,
                                          vk::CommandPoolCreateFlagBits::eTransient);
    auto command = pool->AllocateCommandBuffer(vk::CommandBufferLevel::ePrimary);
    command->Begin();
    command->ImageMemoryBarrier(
        texture.image->GetHandle(), BaseLevel, vk::ImageLayout::eUndefined,
        vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits2::eNone,
        vk::AccessFlagBits2::eNone, vk::PipelineStageFlagBits2::eCopy,
        vk::AccessFlagBits2::eTransferWrite);
    vk::BufferImageCopy copy;
    copy.imageSubresource = vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, 0, 0, 1};
    copy.imageExtent = vk::Extent3D{extent.width, extent.height, 1};
    command->CopyBufferToImage(staging->GetHandle(), texture.image->GetHandle(),
                               vk::ImageLayout::eTransferDstOptimal, copy);
    PrepareTextureForSampling(command.get(), texture.image.get(),
                              vk::ImageLayout::eTransferDstOptimal,
                              vk::PipelineStageFlagBits2::eCopy, vk::AccessFlagBits2::eTransferWrite);
    command->End();
    auto fence = device->CreateFence();
    device->GetQueue(sd::VulkanQueueFamily::Graphics)->Submit(command.get(), fence.get());
    if (fence->Wait() != vk::Result::eSuccess)
    {
        throw std::runtime_error("Texture upload did not complete: " + rad::ToUtf8(path));
    }
    SD_LOG(info, "Texture loaded: {}, {}x{}, {} mip levels, vflip={}",
           rad::ToUtf8(path), extent.width, extent.height, levels, vflip);
    return texture;
}

void PrepareTextureForSampling(sd::VulkanCommandBuffer* command, sd::VulkanImage* image,
                               vk::ImageLayout baseLayout, vk::PipelineStageFlags2 baseStage,
                               vk::AccessFlags2 baseAccess, bool initializedMipmaps)
{
    int32_t width = static_cast<int32_t>(image->GetWidth());
    int32_t height = static_cast<int32_t>(image->GetHeight());
    const uint32_t levels = image->GetMipLevels();
    for (uint32_t level = 1; level < levels; ++level)
    {
        const vk::ImageSubresourceRange sourceRange{
            vk::ImageAspectFlagBits::eColor, level - 1, 1, 0, 1};
        const vk::ImageSubresourceRange destinationRange{
            vk::ImageAspectFlagBits::eColor, level, 1, 0, 1};
        command->ImageMemoryBarrier(
            image->GetHandle(), sourceRange,
            level == 1 ? baseLayout : vk::ImageLayout::eTransferDstOptimal,
            vk::ImageLayout::eTransferSrcOptimal,
            level == 1 ? baseStage : vk::PipelineStageFlags2{vk::PipelineStageFlagBits2::eBlit},
            level == 1 ? baseAccess : vk::AccessFlags2{vk::AccessFlagBits2::eTransferWrite},
            vk::PipelineStageFlagBits2::eBlit, vk::AccessFlagBits2::eTransferRead);
        command->ImageMemoryBarrier(
            image->GetHandle(), destinationRange,
            initializedMipmaps ? vk::ImageLayout::eShaderReadOnlyOptimal : vk::ImageLayout::eUndefined,
            vk::ImageLayout::eTransferDstOptimal,
            initializedMipmaps ? vk::PipelineStageFlagBits2::eAllCommands : vk::PipelineStageFlagBits2::eNone,
            initializedMipmaps ? vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite
                               : vk::AccessFlags2{},
            vk::PipelineStageFlagBits2::eBlit, vk::AccessFlagBits2::eTransferWrite);
        const int32_t nextWidth = std::max(width / 2, 1);
        const int32_t nextHeight = std::max(height / 2, 1);
        vk::ImageBlit blit;
        blit.srcSubresource = vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, level - 1, 0, 1};
        blit.srcOffsets[1] = vk::Offset3D{width, height, 1};
        blit.dstSubresource = vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, level, 0, 1};
        blit.dstOffsets[1] = vk::Offset3D{nextWidth, nextHeight, 1};
        command->BlitImage(image->GetHandle(), vk::ImageLayout::eTransferSrcOptimal,
                            image->GetHandle(), vk::ImageLayout::eTransferDstOptimal,
                            blit, vk::Filter::eLinear);
        command->ImageMemoryBarrier(
            image->GetHandle(), sourceRange, vk::ImageLayout::eTransferSrcOptimal,
            vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eBlit,
            vk::AccessFlagBits2::eTransferRead, vk::PipelineStageFlagBits2::eFragmentShader,
            vk::AccessFlagBits2::eShaderSampledRead);
        width = nextWidth;
        height = nextHeight;
    }
    const vk::ImageSubresourceRange lastRange{vk::ImageAspectFlagBits::eColor, levels - 1, 1, 0, 1};
    command->ImageMemoryBarrier(
        image->GetHandle(), lastRange,
        levels == 1 ? baseLayout : vk::ImageLayout::eTransferDstOptimal,
        vk::ImageLayout::eShaderReadOnlyOptimal,
        levels == 1 ? baseStage : vk::PipelineStageFlags2{vk::PipelineStageFlagBits2::eBlit},
        levels == 1 ? baseAccess : vk::AccessFlags2{vk::AccessFlagBits2::eTransferWrite},
        vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead);
}

} // namespace shadertoy
