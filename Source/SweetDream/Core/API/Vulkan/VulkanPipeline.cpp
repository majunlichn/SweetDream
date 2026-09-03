#include <SweetDream/Core/API/Vulkan/VulkanPipeline.h>
#include <SweetDream/Core/API/Vulkan/VulkanDevice.h>
#include <SweetDream/Core/API/Vulkan/VulkanRenderPass.h>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace sd
{

VulkanShaderModule::VulkanShaderModule(
    rad::Ref<VulkanDevice> device, const vk::ShaderModuleCreateInfo& createInfo) :
    m_device(std::move(device))
{
    if (!m_device)
    {
        throw std::invalid_argument("VulkanShaderModule requires a valid VulkanDevice");
    }

    m_handle =
        m_device->GetHandle().createShaderModule(createInfo, nullptr, GetDispatcher());
}

VulkanShaderModule::~VulkanShaderModule()
{
    if (m_handle)
    {
        m_device->GetHandle().destroyShaderModule(m_handle, nullptr, GetDispatcher());
        m_handle = nullptr;
    }
}

const vk::detail::DispatchLoaderDynamic& VulkanShaderModule::GetDispatcher() const noexcept
{
    return m_device->GetDispatcher();
}

VulkanPipelineLayout::VulkanPipelineLayout(
    rad::Ref<VulkanDevice> device, const vk::PipelineLayoutCreateInfo& createInfo) :
    m_device(std::move(device))
{
    if (!m_device)
    {
        throw std::invalid_argument("VulkanPipelineLayout requires a valid VulkanDevice");
    }

    m_handle =
        m_device->GetHandle().createPipelineLayout(createInfo, nullptr, GetDispatcher());
}

VulkanPipelineLayout::~VulkanPipelineLayout()
{
    if (m_handle)
    {
        m_device->GetHandle().destroyPipelineLayout(m_handle, nullptr, GetDispatcher());
        m_handle = nullptr;
    }
}

const vk::detail::DispatchLoaderDynamic& VulkanPipelineLayout::GetDispatcher() const noexcept
{
    return m_device->GetDispatcher();
}

VulkanPipeline::VulkanPipeline(rad::Ref<VulkanDevice> device) :
    m_device(std::move(device))
{
    if (!m_device)
    {
        throw std::invalid_argument("VulkanPipeline requires a valid VulkanDevice");
    }
}

VulkanPipeline::~VulkanPipeline()
{
    if (m_handle)
    {
        m_device->GetHandle().destroyPipeline(m_handle, nullptr, GetDispatcher());
        m_handle = nullptr;
    }
}

const vk::detail::DispatchLoaderDynamic& VulkanPipeline::GetDispatcher() const noexcept
{
    return m_device->GetDispatcher();
}

VulkanShaderStageInfo::VulkanShaderStageInfo(
    vk::ShaderStageFlagBits stage, rad::Ref<VulkanShaderModule> module,
    std::string_view entryPoint, vk::PipelineShaderStageCreateFlags flags) :
    m_flags(flags),
    m_stage(stage),
    m_module(std::move(module)),
    m_entryPoint(entryPoint)
{
}

VulkanShaderStageInfo::VulkanShaderStageInfo(const VulkanShaderStageInfo& other) :
    m_flags(other.m_flags),
    m_stage(other.m_stage),
    m_module(other.m_module),
    m_entryPoint(other.m_entryPoint),
    m_specMapEntries(other.m_specMapEntries),
    m_specData(other.m_specData)
{
    m_specInfo.mapEntryCount = static_cast<uint32_t>(m_specMapEntries.size());
    m_specInfo.pMapEntries =
        m_specMapEntries.empty() ? nullptr : m_specMapEntries.data();
    m_specInfo.dataSize = m_specData.size();
    m_specInfo.pData = m_specData.empty() ? nullptr : m_specData.data();
}

VulkanShaderStageInfo& VulkanShaderStageInfo::operator=(
    const VulkanShaderStageInfo& other)
{
    if (this != &other)
    {
        m_flags = other.m_flags;
        m_stage = other.m_stage;
        m_module = other.m_module;
        m_entryPoint = other.m_entryPoint;
        m_specMapEntries = other.m_specMapEntries;
        m_specData = other.m_specData;

        m_specInfo.mapEntryCount = static_cast<uint32_t>(m_specMapEntries.size());
        m_specInfo.pMapEntries =
            m_specMapEntries.empty() ? nullptr : m_specMapEntries.data();
        m_specInfo.dataSize = m_specData.size();
        m_specInfo.pData = m_specData.empty() ? nullptr : m_specData.data();
    }
    return *this;
}

VulkanShaderStageInfo::operator vk::PipelineShaderStageCreateInfo() const
{
    assert(m_module);
    assert(!m_entryPoint.empty());

    vk::PipelineShaderStageCreateInfo createInfo = {};
    createInfo.flags = m_flags;
    createInfo.stage = m_stage;
    createInfo.module = m_module->GetHandle();
    createInfo.pName = m_entryPoint.c_str();
    if (!m_specMapEntries.empty() && !m_specData.empty())
    {
        createInfo.pSpecializationInfo = &m_specInfo;
    }
    return createInfo;
}

VulkanGraphicsPipelineCreateInfo::VulkanGraphicsPipelineCreateInfo() = default;

VulkanGraphicsPipelineCreateInfo::~VulkanGraphicsPipelineCreateInfo() = default;

VulkanGraphicsPipeline::VulkanGraphicsPipeline(
    rad::Ref<VulkanDevice> device, const VulkanGraphicsPipelineCreateInfo& info) :
    VulkanPipeline(std::move(device)),
    m_info(info)
{
    if (!m_info.m_layout)
    {
        throw std::invalid_argument("VulkanGraphicsPipeline requires a pipeline layout");
    }
    if (m_info.m_layout->GetDevice() != m_device.get())
    {
        throw std::invalid_argument(
            "VulkanGraphicsPipeline layout belongs to a different VulkanDevice");
    }
    if (m_info.m_renderPass && m_info.m_renderPass->GetDevice() != m_device.get())
    {
        throw std::invalid_argument(
            "VulkanGraphicsPipeline render pass belongs to a different VulkanDevice");
    }
    if (m_info.m_stages.empty())
    {
        throw std::invalid_argument("VulkanGraphicsPipeline requires shader stages");
    }

    std::vector<vk::PipelineShaderStageCreateInfo> stages;
    stages.reserve(m_info.m_stages.size());
    for (const VulkanShaderStageInfo& stage : m_info.m_stages)
    {
        if (!stage.m_module || stage.m_entryPoint.empty())
        {
            throw std::invalid_argument(
                "VulkanGraphicsPipeline shader stages require a module and entry point");
        }
        if (stage.m_module->GetDevice() != m_device.get())
        {
            throw std::invalid_argument(
                "VulkanGraphicsPipeline shader module belongs to a different VulkanDevice");
        }
        stages.emplace_back(stage);
    }

    const auto& vertexInput = m_info.m_vertexInputState;
    vk::PipelineVertexInputStateCreateInfo vertexInputState = {};
    vertexInputState.vertexBindingDescriptionCount =
        static_cast<uint32_t>(vertexInput.bindings.size());
    vertexInputState.pVertexBindingDescriptions = vertexInput.bindings.data();
    vertexInputState.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(vertexInput.attributes.size());
    vertexInputState.pVertexAttributeDescriptions = vertexInput.attributes.data();

    const auto& inputAssembly = m_info.m_inputAssemblyState;
    vk::PipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
    inputAssemblyState.topology = inputAssembly.topology;
    inputAssemblyState.primitiveRestartEnable = inputAssembly.primitiveRestartEnable;

    vk::PipelineTessellationStateCreateInfo tessellationState = {};
    tessellationState.patchControlPoints =
        m_info.m_tessellationState.patchControlPoints;

    const auto& viewport = m_info.m_viewportState;
    const bool hasDynamicViewportCount = viewport.viewportCount == 0;
    const bool hasDynamicScissorCount = viewport.scissorCount == 0;
    if ((hasDynamicViewportCount || hasDynamicScissorCount) &&
        m_device->GetApiVersion().IsLowerThan(VK_API_VERSION_1_3))
    {
        throw std::invalid_argument(
            "VulkanGraphicsPipeline dynamic viewport/scissor counts require Vulkan 1.3");
    }
    if (!hasDynamicViewportCount && !hasDynamicScissorCount &&
        viewport.viewportCount != viewport.scissorCount)
    {
        throw std::invalid_argument(
            "VulkanGraphicsPipeline viewport and scissor counts must match");
    }

    vk::PipelineViewportStateCreateInfo viewportState = {};
    viewportState.viewportCount = viewport.viewportCount;
    viewportState.scissorCount = viewport.scissorCount;

    const auto& rasterization = m_info.m_rasterizationState;
    vk::PipelineRasterizationStateCreateInfo rasterizationState = {};
    rasterizationState.depthClampEnable = rasterization.depthClampEnable;
    rasterizationState.rasterizerDiscardEnable =
        rasterization.rasterizerDiscardEnable;
    rasterizationState.polygonMode = rasterization.polygonMode;
    rasterizationState.cullMode = rasterization.cullMode;
    rasterizationState.frontFace = rasterization.frontFace;
    rasterizationState.depthBiasEnable = rasterization.depthBiasEnable;
    rasterizationState.depthBiasConstantFactor =
        rasterization.depthBiasConstantFactor;
    rasterizationState.depthBiasClamp = rasterization.depthBiasClamp;
    rasterizationState.depthBiasSlopeFactor = rasterization.depthBiasSlopeFactor;
    rasterizationState.lineWidth = rasterization.lineWidth;

    const auto& multisample = m_info.m_multisampleState;
    vk::PipelineMultisampleStateCreateInfo multisampleState = {};
    multisampleState.rasterizationSamples = multisample.rasterizationSamples;
    multisampleState.sampleShadingEnable = multisample.sampleShadingEnable;
    multisampleState.minSampleShading = multisample.minSampleShading;
    multisampleState.pSampleMask =
        multisample.sampleMask.empty() ? nullptr : multisample.sampleMask.data();
    multisampleState.alphaToCoverageEnable = multisample.alphaToCoverageEnable;
    multisampleState.alphaToOneEnable = multisample.alphaToOneEnable;

    const auto& depthStencil = m_info.m_depthStencilState;
    vk::PipelineDepthStencilStateCreateInfo depthStencilState = {};
    depthStencilState.depthTestEnable = depthStencil.depthTestEnable;
    depthStencilState.depthWriteEnable = depthStencil.depthWriteEnable;
    depthStencilState.depthCompareOp = depthStencil.depthCompareOp;
    depthStencilState.depthBoundsTestEnable = depthStencil.depthBoundsTestEnable;
    depthStencilState.stencilTestEnable = depthStencil.stencilTestEnable;
    depthStencilState.front = depthStencil.front;
    depthStencilState.back = depthStencil.back;
    depthStencilState.minDepthBounds = depthStencil.minDepthBounds;
    depthStencilState.maxDepthBounds = depthStencil.maxDepthBounds;

    const auto& colorBlend = m_info.m_colorBlendState;
    vk::PipelineColorBlendStateCreateInfo colorBlendState = {};
    colorBlendState.logicOpEnable = colorBlend.logicOpEnable;
    colorBlendState.logicOp = colorBlend.logicOp;
    colorBlendState.attachmentCount =
        static_cast<uint32_t>(colorBlend.attachments.size());
    colorBlendState.pAttachments =
        colorBlend.attachments.empty() ? nullptr : colorBlend.attachments.data();
    for (std::size_t i = 0; i < std::size(colorBlend.blendConstants); ++i)
    {
        colorBlendState.blendConstants[i] = colorBlend.blendConstants[i];
    }

    const auto& dynamic = m_info.m_dynamicState;
    std::vector<vk::DynamicState> dynamicStates = dynamic.states;
    const auto addDynamicState =
        [&dynamicStates](vk::DynamicState state)
    {
        if (std::find(dynamicStates.begin(), dynamicStates.end(), state) ==
            dynamicStates.end())
        {
            dynamicStates.emplace_back(state);
        }
    };
    const auto hasDynamicState =
        [&dynamicStates](vk::DynamicState state)
    {
        return std::find(dynamicStates.begin(), dynamicStates.end(), state) !=
               dynamicStates.end();
    };
    if (hasDynamicViewportCount)
    {
        if (hasDynamicState(vk::DynamicState::eViewport))
        {
            throw std::invalid_argument(
                "VulkanGraphicsPipeline dynamic-count viewport state conflicts "
                "with fixed-count dynamic state");
        }
        addDynamicState(vk::DynamicState::eViewportWithCount);
    }
    else
    {
        if (hasDynamicState(vk::DynamicState::eViewportWithCount))
        {
            throw std::invalid_argument(
                "VulkanGraphicsPipeline fixed-count viewport state conflicts "
                "with dynamic-count state");
        }
        addDynamicState(vk::DynamicState::eViewport);
    }
    if (hasDynamicScissorCount)
    {
        if (hasDynamicState(vk::DynamicState::eScissor))
        {
            throw std::invalid_argument(
                "VulkanGraphicsPipeline dynamic-count scissor state conflicts "
                "with fixed-count dynamic state");
        }
        addDynamicState(vk::DynamicState::eScissorWithCount);
    }
    else
    {
        if (hasDynamicState(vk::DynamicState::eScissorWithCount))
        {
            throw std::invalid_argument(
                "VulkanGraphicsPipeline fixed-count scissor state conflicts "
                "with dynamic-count state");
        }
        addDynamicState(vk::DynamicState::eScissor);
    }

    vk::PipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    vk::PipelineRenderingCreateInfo renderingInfo = {};
    renderingInfo.viewMask = m_info.m_viewMask;
    renderingInfo.colorAttachmentCount =
        static_cast<uint32_t>(m_info.m_colorAttachmentFormats.size());
    renderingInfo.pColorAttachmentFormats = m_info.m_colorAttachmentFormats.empty()
                                                ? nullptr
                                                : m_info.m_colorAttachmentFormats.data();
    renderingInfo.depthAttachmentFormat = m_info.m_depthAttachmentFormat;
    renderingInfo.stencilAttachmentFormat = m_info.m_stencilAttachmentFormat;

    vk::GraphicsPipelineCreateInfo createInfo = {};
    createInfo.flags = m_info.m_flags;
    createInfo.stageCount = static_cast<uint32_t>(stages.size());
    createInfo.pStages = stages.data();
    createInfo.pVertexInputState = &vertexInputState;
    createInfo.pInputAssemblyState = &inputAssemblyState;
    createInfo.pTessellationState = &tessellationState;
    createInfo.pViewportState = &viewportState;
    createInfo.pRasterizationState = &rasterizationState;
    createInfo.pMultisampleState = &multisampleState;
    createInfo.pDepthStencilState = &depthStencilState;
    createInfo.pColorBlendState = &colorBlendState;
    createInfo.pDynamicState = &dynamicState;
    createInfo.layout = m_info.m_layout->GetHandle();
    createInfo.renderPass =
        m_info.m_renderPass ? m_info.m_renderPass->GetHandle() : vk::RenderPass{};
    createInfo.subpass = m_info.m_subpass;
    if (!m_info.m_renderPass)
    {
        createInfo.pNext = &renderingInfo;
    }

    m_handle =
        m_device->GetHandle()
            .createGraphicsPipeline(nullptr, createInfo, nullptr, GetDispatcher())
            .value;
}

VulkanGraphicsPipeline::~VulkanGraphicsPipeline() = default;

VulkanComputePipelineCreateInfo::VulkanComputePipelineCreateInfo() = default;

VulkanComputePipelineCreateInfo::~VulkanComputePipelineCreateInfo() = default;

VulkanComputePipeline::VulkanComputePipeline(
    rad::Ref<VulkanDevice> device, const VulkanComputePipelineCreateInfo& info) :
    VulkanPipeline(std::move(device)),
    m_info(info)
{
    if (!m_info.m_layout)
    {
        throw std::invalid_argument("VulkanComputePipeline requires a pipeline layout");
    }
    if (m_info.m_layout->GetDevice() != m_device.get())
    {
        throw std::invalid_argument(
            "VulkanComputePipeline layout belongs to a different VulkanDevice");
    }
    if (!m_info.m_stage.m_module || m_info.m_stage.m_entryPoint.empty())
    {
        throw std::invalid_argument(
            "VulkanComputePipeline requires a shader module and entry point");
    }
    if (m_info.m_stage.m_module->GetDevice() != m_device.get())
    {
        throw std::invalid_argument(
            "VulkanComputePipeline shader module belongs to a different VulkanDevice");
    }
    if (m_info.m_stage.m_stage != vk::ShaderStageFlagBits::eCompute)
    {
        throw std::invalid_argument("VulkanComputePipeline requires a compute shader");
    }

    vk::ComputePipelineCreateInfo createInfo = {};
    createInfo.flags = m_info.m_flags;
    createInfo.stage =
        static_cast<vk::PipelineShaderStageCreateInfo>(m_info.m_stage);
    createInfo.layout = m_info.m_layout->GetHandle();

    m_handle =
        m_device->GetHandle()
            .createComputePipeline(nullptr, createInfo, nullptr, GetDispatcher())
            .value;
    m_bindPoint = vk::PipelineBindPoint::eCompute;
}

VulkanComputePipeline::~VulkanComputePipeline() = default;

} // namespace sd
