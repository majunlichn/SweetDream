#pragma once

#include <SweetDream/GPU/Vulkan/VulkanImage.h>

#include <rad/Core/FilePath.h>

namespace shadertoy
{

struct Texture
{
    rad::Ref<sd::VulkanImage> image;
    rad::Ref<sd::VulkanImageView> view;
};

[[nodiscard]] Texture LoadTexture(sd::VulkanDevice* device, const rad::FilePath& path,
                                  bool vflip, bool mipmaps);

} // namespace shadertoy
