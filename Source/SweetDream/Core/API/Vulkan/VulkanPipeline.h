#pragma once

#include <SweetDream/Core/API/Vulkan/VulkanCommon.h>

#include <cstring>

namespace sd
{

class VulkanDevice;
class VulkanRenderPass;

[[nodiscard]] constexpr vk::PipelineColorBlendAttachmentState
MakeOpaqueColorBlendAttachmentState(
    vk::ColorComponentFlags colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA) noexcept
{
    vk::PipelineColorBlendAttachmentState state;
    state.blendEnable = VK_FALSE;
    state.colorWriteMask = colorWriteMask;
    return state;
}

class VulkanShaderModule : public rad::RefCounted<VulkanShaderModule>
{
public:
    VulkanShaderModule(rad::Ref<VulkanDevice> device,
                       const vk::ShaderModuleCreateInfo& createInfo);
    ~VulkanShaderModule();

    VulkanShaderModule(const VulkanShaderModule&) = delete;
    VulkanShaderModule& operator=(const VulkanShaderModule&) = delete;

    [[nodiscard]] VulkanDevice* GetDevice() const noexcept { return m_device.get(); }
    [[nodiscard]] const vk::detail::DispatchLoaderDynamic& GetDispatcher() const noexcept;
    [[nodiscard]] const vk::ShaderModule& GetHandle() const noexcept { return m_handle; }

private:
    rad::Ref<VulkanDevice> m_device;
    vk::ShaderModule m_handle = nullptr;
}; // class VulkanShaderModule

class VulkanPipelineLayout : public rad::RefCounted<VulkanPipelineLayout>
{
public:
    VulkanPipelineLayout(rad::Ref<VulkanDevice> device,
                         const vk::PipelineLayoutCreateInfo& createInfo);
    ~VulkanPipelineLayout();

    VulkanPipelineLayout(const VulkanPipelineLayout&) = delete;
    VulkanPipelineLayout& operator=(const VulkanPipelineLayout&) = delete;

    [[nodiscard]] VulkanDevice* GetDevice() const noexcept { return m_device.get(); }
    [[nodiscard]] const vk::detail::DispatchLoaderDynamic& GetDispatcher() const noexcept;
    [[nodiscard]] const vk::PipelineLayout& GetHandle() const noexcept { return m_handle; }

private:
    rad::Ref<VulkanDevice> m_device;
    vk::PipelineLayout m_handle = nullptr;
}; // class VulkanPipelineLayout

class VulkanPipeline : public rad::RefCounted<VulkanPipeline>
{
public:
    explicit VulkanPipeline(rad::Ref<VulkanDevice> device);
    virtual ~VulkanPipeline();

    VulkanPipeline(const VulkanPipeline&) = delete;
    VulkanPipeline& operator=(const VulkanPipeline&) = delete;

    [[nodiscard]] VulkanDevice* GetDevice() const noexcept { return m_device.get(); }
    [[nodiscard]] const vk::detail::DispatchLoaderDynamic& GetDispatcher() const noexcept;
    [[nodiscard]] const vk::Pipeline& GetHandle() const noexcept { return m_handle; }
    [[nodiscard]] vk::PipelineBindPoint GetBindPoint() const noexcept
    {
        return m_bindPoint;
    }

protected:
    rad::Ref<VulkanDevice> m_device;
    vk::Pipeline m_handle = nullptr;
    vk::PipelineBindPoint m_bindPoint = vk::PipelineBindPoint::eGraphics;
}; // class VulkanPipeline

struct VulkanShaderStageInfo : public rad::RefCounted<VulkanShaderStageInfo>
{
public:
    VulkanShaderStageInfo() = default;
    VulkanShaderStageInfo(vk::ShaderStageFlagBits stage,
                          rad::Ref<VulkanShaderModule> module,
                          std::string_view entryPoint = "main",
                          vk::PipelineShaderStageCreateFlags flags = {});
    VulkanShaderStageInfo(const VulkanShaderStageInfo& other);
    VulkanShaderStageInfo& operator=(const VulkanShaderStageInfo& other);
    ~VulkanShaderStageInfo() = default;

    template <typename T>
    void SetSpecialization(uint32_t constantID, const T& value)
    {
        const std::size_t offset = m_specData.size();
        m_specData.resize(offset + sizeof(value));
        std::memcpy(m_specData.data() + offset, &value, sizeof(value));
        m_specMapEntries.emplace_back(constantID, static_cast<uint32_t>(offset),
                                      sizeof(T));

        m_specInfo.mapEntryCount = static_cast<uint32_t>(m_specMapEntries.size());
        m_specInfo.pMapEntries = m_specMapEntries.data();
        m_specInfo.dataSize = m_specData.size();
        m_specInfo.pData = m_specData.data();
    }

    operator vk::PipelineShaderStageCreateInfo() const;

    vk::PipelineShaderStageCreateFlags m_flags = {};
    vk::ShaderStageFlagBits m_stage = vk::ShaderStageFlagBits::eCompute;
    rad::Ref<VulkanShaderModule> m_module;
    std::string m_entryPoint;
    vk::SpecializationInfo m_specInfo = {};
    std::vector<vk::SpecializationMapEntry> m_specMapEntries;
    std::vector<uint8_t> m_specData;
}; // struct VulkanShaderStageInfo

struct VulkanGraphicsPipelineCreateInfo
    : public rad::RefCounted<VulkanGraphicsPipelineCreateInfo>
{
    VulkanGraphicsPipelineCreateInfo();
    ~VulkanGraphicsPipelineCreateInfo();

    vk::PipelineCreateFlags m_flags = {};
    std::vector<VulkanShaderStageInfo> m_stages;

    struct VertexInputState
    {
        std::vector<vk::VertexInputBindingDescription> bindings;
        std::vector<vk::VertexInputAttributeDescription> attributes;
    } m_vertexInputState;

    struct InputAssemblyState
    {
        vk::PrimitiveTopology topology = vk::PrimitiveTopology::eTriangleList;
        vk::Bool32 primitiveRestartEnable = VK_FALSE;
    } m_inputAssemblyState;

    struct TessellationState
    {
        uint32_t patchControlPoints = 0;
    } m_tessellationState;

    struct ViewportState
    {
        // Zero selects dynamic viewport/scissor count.
        uint32_t viewportCount = 0;
        uint32_t scissorCount = 0;
    } m_viewportState;

    struct RasterizationState
    {
        vk::Bool32 depthClampEnable = VK_FALSE;
        vk::Bool32 rasterizerDiscardEnable = VK_FALSE;
        vk::PolygonMode polygonMode = vk::PolygonMode::eFill;
        vk::CullModeFlags cullMode = {};
        vk::FrontFace frontFace = vk::FrontFace::eCounterClockwise;
        vk::Bool32 depthBiasEnable = VK_FALSE;
        float depthBiasConstantFactor = 0.0f;
        float depthBiasClamp = 0.0f;
        float depthBiasSlopeFactor = 0.0f;
        float lineWidth = 1.0f;
    } m_rasterizationState;

    struct MultisampleState
    {
        vk::SampleCountFlagBits rasterizationSamples = vk::SampleCountFlagBits::e1;
        vk::Bool32 sampleShadingEnable = VK_FALSE;
        float minSampleShading = 1.0f;
        std::vector<vk::SampleMask> sampleMask;
        vk::Bool32 alphaToCoverageEnable = VK_FALSE;
        vk::Bool32 alphaToOneEnable = VK_FALSE;
    } m_multisampleState;

    struct DepthStencilState
    {
        vk::Bool32 depthTestEnable = VK_FALSE;
        vk::Bool32 depthWriteEnable = VK_FALSE;
        vk::CompareOp depthCompareOp = vk::CompareOp::eNever;
        vk::Bool32 depthBoundsTestEnable = VK_FALSE;
        vk::Bool32 stencilTestEnable = VK_FALSE;
        vk::StencilOpState front = {};
        vk::StencilOpState back = {};
        float minDepthBounds = 0.0f;
        float maxDepthBounds = 1.0f;
    } m_depthStencilState;

    struct ColorBlendState
    {
        vk::Bool32 logicOpEnable = VK_FALSE;
        vk::LogicOp logicOp = vk::LogicOp::eCopy;
        std::vector<vk::PipelineColorBlendAttachmentState> attachments;
        float blendConstants[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    } m_colorBlendState;

    struct DynamicState
    {
        // Viewport/scissor values are always dynamic.
        std::vector<vk::DynamicState> states;
    } m_dynamicState;

    rad::Ref<VulkanPipelineLayout> m_layout;
    rad::Ref<VulkanRenderPass> m_renderPass;
    uint32_t m_subpass = 0;

    // Dynamic Rendering (VK_KHR_dynamic_rendering / Vulkan 1.3 core).
    // If m_renderPass is null, these attachment formats populate
    // vk::PipelineRenderingCreateInfo.
    std::vector<vk::Format> m_colorAttachmentFormats;
    vk::Format m_depthAttachmentFormat = vk::Format::eUndefined;
    vk::Format m_stencilAttachmentFormat = vk::Format::eUndefined;
    uint32_t m_viewMask = 0;
}; // struct VulkanGraphicsPipelineCreateInfo

class VulkanGraphicsPipeline : public VulkanPipeline
{
public:
    VulkanGraphicsPipeline(rad::Ref<VulkanDevice> device,
                           const VulkanGraphicsPipelineCreateInfo& info);
    ~VulkanGraphicsPipeline() override;

private:
    VulkanGraphicsPipelineCreateInfo m_info;
}; // class VulkanGraphicsPipeline

struct VulkanComputePipelineCreateInfo
    : public rad::RefCounted<VulkanComputePipelineCreateInfo>
{
    VulkanComputePipelineCreateInfo();
    ~VulkanComputePipelineCreateInfo();

    vk::PipelineCreateFlags m_flags = {};
    VulkanShaderStageInfo m_stage;
    rad::Ref<VulkanPipelineLayout> m_layout;
}; // struct VulkanComputePipelineCreateInfo

class VulkanComputePipeline : public VulkanPipeline
{
public:
    VulkanComputePipeline(rad::Ref<VulkanDevice> device,
                          const VulkanComputePipelineCreateInfo& info);
    ~VulkanComputePipeline() override;

private:
    VulkanComputePipelineCreateInfo m_info;
}; // class VulkanComputePipeline

} // namespace sd
