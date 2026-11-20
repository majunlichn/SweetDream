#include "ShaderToyTexture.h"

#include <SweetDream/GPU/Vulkan/VulkanBuffer.h>
#include <SweetDream/GPU/Vulkan/VulkanCommand.h>
#include <SweetDream/GPU/Vulkan/VulkanFence.h>
#include <SweetDream/GPU/Vulkan/VulkanMipmap.h>
#include <SweetDream/GPU/Vulkan/VulkanQueue.h>
#include <SweetDream/Core/IO/Logging.h>

#include <rad/IO/Image.h>
#include <rad/IO/File.h>

#include <algorithm>
#include <stdexcept>

namespace shadertoy
{
namespace
{

constexpr vk::ImageSubresourceRange BaseLevel{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
constexpr vk::Format TextureFormat = vk::Format::eR8G8B8A8Unorm;

} // namespace

Texture LoadTexture(sd::VulkanDevice* device, const rad::FilePath& path,
                    bool vflip, bool mipmaps)
{
    const auto data = rad::File::ReadAllBytes(path);
    if (!data)
    {
        throw std::runtime_error("Cannot read texture: " + rad::ToUtf8(path) +
                                 "; run download_resources.py from the ShaderToy asset directory");
    }
    const auto& fileBytes = data.assume_value();
    auto pixels = rad::ImageUnorm8::LoadFromMemory(fileBytes.data(), fileBytes.size(), 4);
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
    const uint32_t levels = mipmaps ? sd::VulkanMipmap::GetMipLevelCount(extent) : 1;
    auto usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
    if (levels > 1)
    {
        usage |= vk::ImageUsageFlagBits::eStorage;
    }
    Texture texture;
    texture.image = device->CreateImage2D(
        extent.width, extent.height, TextureFormat, usage, VMA_MEMORY_USAGE_AUTO, 0, levels);
    texture.view = texture.image->CreateView2D(0, levels);
    sd::VulkanMipmap mipmap(device);
    mipmap.SetImage(texture.image.get());

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
    mipmap.Execute(
        command.get(),
        {vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits2::eCopy,
         vk::AccessFlagBits2::eTransferWrite},
        {},
        {vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader,
         vk::AccessFlagBits2::eShaderSampledRead});
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

} // namespace shadertoy
