#pragma once

#include <SweetDream/Core/API/Vulkan/VulkanCommand.h>
#include <SweetDream/Core/API/Vulkan/VulkanImage.h>

#include <rad/System/FileSystem.h>

namespace shadertoy
{

struct Texture
{
    rad::Ref<sd::VulkanImage> image;
    rad::Ref<sd::VulkanImageView> view;
};

[[nodiscard]] uint32_t GetMipLevelCount(vk::Extent2D extent);
void CheckMipmapSupport(sd::VulkanDevice* device, vk::Format format);
[[nodiscard]] Texture LoadTexture(sd::VulkanDevice* device, const rad::FilePath& path,
                                  bool vflip, bool mipmaps);

// Base level contains new pixels. Other levels are undefined on upload or shader-readable
// when regenerating a Buffer. All levels are shader-readable on return.
void PrepareTextureForSampling(sd::VulkanCommandBuffer* command, sd::VulkanImage* image,
                               vk::ImageLayout baseLayout, vk::PipelineStageFlags2 baseStage,
                               vk::AccessFlags2 baseAccess, bool initializedMipmaps = false);

} // namespace shadertoy
