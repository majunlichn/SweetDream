#pragma once

#include "ShaderToyConfig.h"
#include "ShaderToyTexture.h"

#include <SweetDream/GPU/Vulkan/VulkanBuffer.h>
#include <SweetDream/GPU/Vulkan/VulkanCommand.h>
#include <SweetDream/GPU/Vulkan/VulkanDescriptor.h>
#include <SweetDream/GPU/Vulkan/VulkanImage.h>
#include <SweetDream/GPU/Vulkan/VulkanMipmap.h>
#include <SweetDream/GPU/Vulkan/VulkanPipeline.h>
#include <SweetDream/GPU/Vulkan/VulkanSampler.h>

#include <cstddef>
#include <memory>

namespace shadertoy
{

using Float4 = std::array<float, 4>;

struct CompiledShaders
{
    std::vector<uint32_t> vertex;
    std::vector<std::vector<uint32_t>> fragments;
};

[[nodiscard]] CompiledShaders CompileShaders(const EffectConfig& config);

struct FrameData
{
    uint32_t index = 0;
    float time = 0.0f;
    float delta = 0.0f;
    float frameRate = 60.0f;
    // Normalized bottom-left coordinates; zw signs encode down/click respectively.
    Float4 mouse = {};
    Float4 date = {};
};

class Renderer
{
public:
    Renderer(sd::VulkanDevice* device, EffectConfig config, CompiledShaders shaders,
             uint32_t frameSlotCount);

    // Caller must wait for all previous rendering before resizing or destroying.
    void Resize(vk::Extent2D extent, vk::Format screenFormat);
    // The frame slot must be idle; screen must already be a color attachment.
    void Render(sd::VulkanCommandBuffer* command, sd::VulkanImageView* screen,
                uint32_t frameSlot, const FrameData& frame);

private:
    struct RenderTarget
    {
        rad::Ref<sd::VulkanImage> image;
        rad::Ref<sd::VulkanImageView> attachmentView;
        rad::Ref<sd::VulkanImageView> sampledView;
        std::unique_ptr<sd::VulkanMipmap> mipmap;
        float time = 0.0f;
    };

    struct InputChannel
    {
        rad::Ref<sd::VulkanSampler> sampler;
        Texture texture;
        std::optional<std::size_t> sourcePassIndex;
        bool samplePreviousFrame = false;
    };

    struct Pass
    {
        rad::Ref<sd::VulkanShaderModule> shader;
        rad::Ref<sd::VulkanGraphicsPipeline> pipeline;
        std::vector<rad::Ref<sd::VulkanDescriptorSet>> descriptorSets;
        std::vector<std::array<vk::ImageView, 4>> boundViews;
        std::array<InputChannel, 4> inputs;
        std::array<RenderTarget, 2> targets;
        vk::Extent2D extent;
        bool mipmaps = false;
    };

    void CreateRenderTarget(RenderTarget& target, vk::Extent2D extent, bool mipmaps = false);
    void InitializeTargets();
    void CreatePipeline(std::size_t passIndex, vk::Format format);
    void UpdatePassResources(std::size_t passIndex, uint32_t frameSlot, const FrameData& frame);

    rad::Ref<sd::VulkanDevice> m_device;
    EffectConfig m_config;
    uint32_t m_frameSlotCount;
    vk::DeviceSize m_uniformStride = 0;
    rad::Ref<sd::VulkanShaderModule> m_vertex;
    rad::Ref<sd::VulkanDescriptorSetLayout> m_setLayout;
    rad::Ref<sd::VulkanPipelineLayout> m_layout;
    rad::Ref<sd::VulkanDescriptorPool> m_descriptorPool;
    rad::Ref<sd::VulkanBuffer> m_uniformBuffer;
    std::byte* m_uniformMapping = nullptr;
    std::vector<Pass> m_passes;
    RenderTarget m_black;
    vk::Format m_screenFormat = vk::Format::eUndefined;
};

} // namespace shadertoy
