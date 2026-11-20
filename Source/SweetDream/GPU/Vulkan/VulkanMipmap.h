#pragma once

#include <SweetDream/GPU/Vulkan/VulkanCommon.h>

namespace sd
{

class VulkanCommandBuffer;
class VulkanComputePipeline;
class VulkanDescriptorSet;
class VulkanDescriptorSetLayout;
class VulkanDevice;
class VulkanImage;
class VulkanImageView;
class VulkanPipelineLayout;

class VulkanMipmap
{
public:
    struct ImageState
    {
        vk::ImageLayout layout = vk::ImageLayout::eUndefined;
        vk::PipelineStageFlags2 stage = {};
        vk::AccessFlags2 access = {};
    };

    explicit VulkanMipmap(VulkanDevice* device);
    ~VulkanMipmap();

    VulkanMipmap(const VulkanMipmap&) = delete;
    VulkanMipmap& operator=(const VulkanMipmap&) = delete;

    [[nodiscard]] static uint32_t GetMipLevelCount(vk::Extent2D extent) noexcept;

    // Single-layer, single-sample, optimal 2D color image. Mip chains require
    // storage usage and RGBA8 UNORM or RGBA32F. A single level is only transitioned.
    // Retains the image. Keep this generator and its binding alive while recorded
    // commands can still execute; SetImage must not replace an in-use binding.
    void SetImage(VulkanImage* image);

    // Record outside rendering on a queue supporting compute (for mip chains).
    // The device must have synchronization2 enabled. No submission or queue ownership
    // transfer is performed. Execute does not track layouts or update descriptors.
    // baseStateBefore describes the initialized mip 0. lowerStateBefore describes
    // mip 1 onward as a group; pass {} for unused levels. stateAfter describes
    // the next use of the whole chain.
    void Execute(VulkanCommandBuffer* command, ImageState baseStateBefore,
                 ImageState lowerStateBefore, ImageState stateAfter) const;

private:
    struct Pipeline
    {
        rad::Ref<VulkanDescriptorSetLayout> setLayout;
        rad::Ref<VulkanPipelineLayout> layout;
        rad::Ref<VulkanComputePipeline> pipeline;
        vk::Format format = vk::Format::eUndefined;
    };

    struct Dispatch
    {
        rad::Ref<VulkanDescriptorSet> set;
        uint32_t levelCount;
        uint32_t groupsX;
        uint32_t groupsY;
        bool fast;
    };

    rad::Ref<VulkanDevice> m_device;
    Pipeline m_generalPipeline;
    Pipeline m_fastPipeline;
    rad::Ref<VulkanImage> m_image;
    std::vector<rad::Ref<VulkanImageView>> m_views;
    std::vector<Dispatch> m_dispatches;
}; // class VulkanMipmap

} // namespace sd
