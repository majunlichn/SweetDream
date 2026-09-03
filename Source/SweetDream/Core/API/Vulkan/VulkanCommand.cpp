#include <SweetDream/Core/API/Vulkan/VulkanCommand.h>
#include <SweetDream/Core/API/Vulkan/VulkanImage.h>

#include <rad/Container/SmallVector.h>

#include <stdexcept>
#include <utility>

namespace sd
{

VulkanCommandPool::VulkanCommandPool(rad::Ref<VulkanDevice> device,
                                     VulkanQueueFamily queueFamily,
                                     vk::CommandPoolCreateFlags flags) :
    m_device(std::move(device)),
    m_queueFamily(queueFamily)
{
    if (!m_device)
    {
        throw std::invalid_argument("VulkanCommandPool requires a valid VulkanDevice");
    }
    if (!m_device->HasQueueFamily(m_queueFamily))
    {
        throw std::invalid_argument("VulkanCommandPool requires an available queue family");
    }

    vk::CommandPoolCreateInfo commandPoolCreateInfo;
    commandPoolCreateInfo.flags = flags;
    commandPoolCreateInfo.queueFamilyIndex = m_device->GetQueueFamilyIndex(m_queueFamily);
    m_handle = m_device->GetHandle().createCommandPool(commandPoolCreateInfo, nullptr,
                                                        GetDispatcher());
}

VulkanCommandPool::~VulkanCommandPool()
{
    if (m_handle)
    {
        m_device->GetHandle().destroyCommandPool(m_handle, nullptr, GetDispatcher());
        m_handle = nullptr;
    }
}

const vk::detail::DispatchLoaderDynamic& VulkanCommandPool::GetDispatcher() const noexcept
{
    return m_device->GetDispatcher();
}

std::vector<rad::Ref<VulkanCommandBuffer>> VulkanCommandPool::AllocateCommandBuffers(
    vk::CommandBufferLevel level, uint32_t count)
{
    if (count == 0)
    {
        throw std::invalid_argument("Command buffer allocation count must be greater than zero");
    }

    vk::CommandBufferAllocateInfo allocateInfo;
    allocateInfo.commandPool = m_handle;
    allocateInfo.level = level;
    allocateInfo.commandBufferCount = count;

    const std::vector<vk::CommandBuffer> handles =
        m_device->GetHandle().allocateCommandBuffers(allocateInfo, GetDispatcher());

    std::vector<rad::Ref<VulkanCommandBuffer>> commandBuffers;
    commandBuffers.reserve(handles.size());
    try
    {
        for (vk::CommandBuffer handle : handles)
        {
            commandBuffers.emplace_back(new VulkanCommandBuffer(this, handle));
        }
    }
    catch (...)
    {
        const std::size_t wrappedCount = commandBuffers.size();
        if (wrappedCount < handles.size())
        {
            m_device->GetHandle().freeCommandBuffers(
                m_handle, static_cast<uint32_t>(handles.size() - wrappedCount),
                handles.data() + wrappedCount, GetDispatcher());
        }
        throw;
    }
    return commandBuffers;
}

rad::Ref<VulkanCommandBuffer> VulkanCommandPool::AllocateCommandBuffer(
    vk::CommandBufferLevel level)
{
    std::vector<rad::Ref<VulkanCommandBuffer>> commandBuffers =
        AllocateCommandBuffers(level, 1);
    return std::move(commandBuffers.front());
}

VulkanCommandBuffer::VulkanCommandBuffer(rad::Ref<VulkanCommandPool> cmdPool,
                                         vk::CommandBuffer cmdBufferHandle) :
    m_cmdPool(std::move(cmdPool)),
    m_handle(cmdBufferHandle)
{
    if (!m_cmdPool)
    {
        throw std::invalid_argument("VulkanCommandBuffer requires a valid VulkanCommandPool");
    }
    if (!m_handle)
    {
        throw std::invalid_argument("VulkanCommandBuffer requires a valid command buffer");
    }
}

VulkanCommandBuffer::~VulkanCommandBuffer()
{
    if (m_handle)
    {
        GetDevice()->GetHandle().freeCommandBuffers(m_cmdPool->GetHandle(), 1, &m_handle,
                                                    GetDispatcher());
        m_handle = nullptr;
    }
}

const vk::detail::DispatchLoaderDynamic& VulkanCommandBuffer::GetDispatcher() const noexcept
{
    return m_cmdPool->GetDispatcher();
}

void VulkanCommandBuffer::Begin(
    vk::CommandBufferUsageFlags flags,
    const vk::CommandBufferInheritanceInfo* pInheritanceInfo)
{
    vk::CommandBufferBeginInfo beginInfo;
    beginInfo.flags = flags;
    beginInfo.pInheritanceInfo = pInheritanceInfo;
    m_handle.begin(beginInfo, GetDispatcher());
}

void VulkanCommandBuffer::End()
{
    m_handle.end(GetDispatcher());
}

void VulkanCommandBuffer::Reset(vk::CommandBufferResetFlags flags)
{
    m_handle.reset(flags, GetDispatcher());
}

void VulkanCommandBuffer::BindPipeline(vk::PipelineBindPoint pipelineBindPoint,
                                       vk::Pipeline pipeline)
{
    m_handle.bindPipeline(pipelineBindPoint, pipeline, GetDispatcher());
}

void VulkanCommandBuffer::BindDescriptorSets(
    vk::PipelineBindPoint pipelineBindPoint, vk::PipelineLayout layout,
    uint32_t firstSet, vk::ArrayProxy<const vk::DescriptorSet> descriptorSets,
    vk::ArrayProxy<const uint32_t> dynamicOffsets)
{
    m_handle.bindDescriptorSets(pipelineBindPoint, layout, firstSet, descriptorSets,
                                dynamicOffsets, GetDispatcher());
}

void VulkanCommandBuffer::BindIndexBuffer(vk::Buffer buffer, vk::DeviceSize offset,
                                          vk::IndexType indexType)
{
    m_handle.bindIndexBuffer(buffer, offset, indexType, GetDispatcher());
}

void VulkanCommandBuffer::BindVertexBuffers(
    uint32_t firstBinding, vk::ArrayProxy<const vk::Buffer> buffers,
    vk::ArrayProxy<const vk::DeviceSize> offsets)
{
    m_handle.bindVertexBuffers(firstBinding, buffers, offsets, GetDispatcher());
}

void VulkanCommandBuffer::SetViewport(
    uint32_t firstViewport, vk::ArrayProxy<const vk::Viewport> viewports)
{
    m_handle.setViewport(firstViewport, viewports, GetDispatcher());
}

void VulkanCommandBuffer::SetScissor(
    uint32_t firstScissor, vk::ArrayProxy<const vk::Rect2D> scissors)
{
    m_handle.setScissor(firstScissor, scissors, GetDispatcher());
}

void VulkanCommandBuffer::SetLineWidth(float lineWidth)
{
    m_handle.setLineWidth(lineWidth, GetDispatcher());
}

void VulkanCommandBuffer::SetDepthBias(
    float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor)
{
    m_handle.setDepthBias(depthBiasConstantFactor, depthBiasClamp,
                          depthBiasSlopeFactor, GetDispatcher());
}

void VulkanCommandBuffer::SetBlendConstants(
    const std::array<float, 4>& blendConstants)
{
    m_handle.setBlendConstants(blendConstants.data(), GetDispatcher());
}

void VulkanCommandBuffer::SetDepthBounds(float minDepthBounds, float maxDepthBounds)
{
    m_handle.setDepthBounds(minDepthBounds, maxDepthBounds, GetDispatcher());
}

void VulkanCommandBuffer::SetStencilCompareMask(vk::StencilFaceFlags faceMask,
                                                uint32_t compareMask)
{
    m_handle.setStencilCompareMask(faceMask, compareMask, GetDispatcher());
}

void VulkanCommandBuffer::SetStencilWriteMask(vk::StencilFaceFlags faceMask,
                                              uint32_t writeMask)
{
    m_handle.setStencilWriteMask(faceMask, writeMask, GetDispatcher());
}

void VulkanCommandBuffer::SetStencilReference(vk::StencilFaceFlags faceMask,
                                              uint32_t reference)
{
    m_handle.setStencilReference(faceMask, reference, GetDispatcher());
}

void VulkanCommandBuffer::Draw(uint32_t vertexCount, uint32_t instanceCount,
                               uint32_t firstVertex, uint32_t firstInstance)
{
    m_handle.draw(vertexCount, instanceCount, firstVertex, firstInstance,
                  GetDispatcher());
}

void VulkanCommandBuffer::DrawIndexed(
    uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex,
    int32_t vertexOffset, uint32_t firstInstance)
{
    m_handle.drawIndexed(indexCount, instanceCount, firstIndex, vertexOffset,
                         firstInstance, GetDispatcher());
}

void VulkanCommandBuffer::DrawIndirect(
    vk::Buffer buffer, vk::DeviceSize offset, uint32_t drawCount, uint32_t stride)
{
    m_handle.drawIndirect(buffer, offset, drawCount, stride, GetDispatcher());
}

void VulkanCommandBuffer::DrawIndexedIndirect(
    vk::Buffer buffer, vk::DeviceSize offset, uint32_t drawCount, uint32_t stride)
{
    m_handle.drawIndexedIndirect(buffer, offset, drawCount, stride, GetDispatcher());
}

void VulkanCommandBuffer::Dispatch(uint32_t groupCountX, uint32_t groupCountY,
                                   uint32_t groupCountZ)
{
    m_handle.dispatch(groupCountX, groupCountY, groupCountZ, GetDispatcher());
}

void VulkanCommandBuffer::DispatchIndirect(vk::Buffer buffer,
                                           vk::DeviceSize offset)
{
    m_handle.dispatchIndirect(buffer, offset, GetDispatcher());
}

void VulkanCommandBuffer::CopyBuffer(
    vk::Buffer srcBuffer, vk::Buffer dstBuffer,
    vk::ArrayProxy<const vk::BufferCopy> regions)
{
    m_handle.copyBuffer(srcBuffer, dstBuffer, regions, GetDispatcher());
}

void VulkanCommandBuffer::CopyImage(
    vk::Image srcImage, vk::ImageLayout srcImageLayout, vk::Image dstImage,
    vk::ImageLayout dstImageLayout, vk::ArrayProxy<const vk::ImageCopy> regions)
{
    m_handle.copyImage(srcImage, srcImageLayout, dstImage, dstImageLayout, regions,
                       GetDispatcher());
}

void VulkanCommandBuffer::BlitImage(
    vk::Image srcImage, vk::ImageLayout srcImageLayout, vk::Image dstImage,
    vk::ImageLayout dstImageLayout, vk::ArrayProxy<const vk::ImageBlit> regions,
    vk::Filter filter)
{
    m_handle.blitImage(srcImage, srcImageLayout, dstImage, dstImageLayout, regions,
                       filter, GetDispatcher());
}

void VulkanCommandBuffer::CopyBufferToImage(
    vk::Buffer srcBuffer, vk::Image dstImage, vk::ImageLayout dstImageLayout,
    vk::ArrayProxy<const vk::BufferImageCopy> regions)
{
    m_handle.copyBufferToImage(srcBuffer, dstImage, dstImageLayout, regions,
                               GetDispatcher());
}

void VulkanCommandBuffer::CopyImageToBuffer(
    vk::Image srcImage, vk::ImageLayout srcImageLayout, vk::Buffer dstBuffer,
    vk::ArrayProxy<const vk::BufferImageCopy> regions)
{
    m_handle.copyImageToBuffer(srcImage, srcImageLayout, dstBuffer, regions,
                               GetDispatcher());
}

void VulkanCommandBuffer::UpdateBuffer(
    vk::Buffer dstBuffer, vk::DeviceSize dstOffset, vk::DeviceSize dataSize,
    const void* data)
{
    m_handle.updateBuffer(dstBuffer, dstOffset, dataSize, data, GetDispatcher());
}

void VulkanCommandBuffer::FillBuffer(vk::Buffer dstBuffer,
                                     vk::DeviceSize dstOffset,
                                     vk::DeviceSize size, uint32_t data)
{
    m_handle.fillBuffer(dstBuffer, dstOffset, size, data, GetDispatcher());
}

void VulkanCommandBuffer::ClearColorImage(
    vk::Image image, vk::ImageLayout imageLayout,
    const vk::ClearColorValue& color,
    vk::ArrayProxy<const vk::ImageSubresourceRange> ranges)
{
    m_handle.clearColorImage(image, imageLayout, color, ranges, GetDispatcher());
}

void VulkanCommandBuffer::ClearDepthStencilImage(
    vk::Image image, vk::ImageLayout imageLayout,
    const vk::ClearDepthStencilValue& depthStencil,
    vk::ArrayProxy<const vk::ImageSubresourceRange> ranges)
{
    m_handle.clearDepthStencilImage(image, imageLayout, depthStencil, ranges,
                                    GetDispatcher());
}

void VulkanCommandBuffer::ClearAttachments(
    vk::ArrayProxy<const vk::ClearAttachment> attachments,
    vk::ArrayProxy<const vk::ClearRect> rects)
{
    m_handle.clearAttachments(attachments, rects, GetDispatcher());
}

void VulkanCommandBuffer::ResolveImage(
    vk::Image srcImage, vk::ImageLayout srcImageLayout, vk::Image dstImage,
    vk::ImageLayout dstImageLayout,
    vk::ArrayProxy<const vk::ImageResolve> regions)
{
    m_handle.resolveImage(srcImage, srcImageLayout, dstImage, dstImageLayout,
                          regions, GetDispatcher());
}

void VulkanCommandBuffer::PipelineBarrier(
    vk::PipelineStageFlags srcStageMask, vk::PipelineStageFlags dstStageMask,
    vk::DependencyFlags dependencyFlags,
    vk::ArrayProxy<const vk::MemoryBarrier> memoryBarriers,
    vk::ArrayProxy<const vk::BufferMemoryBarrier> bufferMemoryBarriers,
    vk::ArrayProxy<const vk::ImageMemoryBarrier> imageMemoryBarriers)
{
    m_handle.pipelineBarrier(srcStageMask, dstStageMask, dependencyFlags,
                             memoryBarriers, bufferMemoryBarriers,
                             imageMemoryBarriers, GetDispatcher());
}

void VulkanCommandBuffer::SetEvent(vk::Event event,
                                   vk::PipelineStageFlags stageMask)
{
    m_handle.setEvent(event, stageMask, GetDispatcher());
}

void VulkanCommandBuffer::ResetEvent(vk::Event event,
                                     vk::PipelineStageFlags stageMask)
{
    m_handle.resetEvent(event, stageMask, GetDispatcher());
}

void VulkanCommandBuffer::WaitEvents(
    vk::ArrayProxy<const vk::Event> events,
    vk::PipelineStageFlags srcStageMask, vk::PipelineStageFlags dstStageMask,
    vk::ArrayProxy<const vk::MemoryBarrier> memoryBarriers,
    vk::ArrayProxy<const vk::BufferMemoryBarrier> bufferMemoryBarriers,
    vk::ArrayProxy<const vk::ImageMemoryBarrier> imageMemoryBarriers)
{
    m_handle.waitEvents(events, srcStageMask, dstStageMask, memoryBarriers,
                        bufferMemoryBarriers, imageMemoryBarriers, GetDispatcher());
}

void VulkanCommandBuffer::BeginQuery(vk::QueryPool queryPool, uint32_t query,
                                     vk::QueryControlFlags flags)
{
    m_handle.beginQuery(queryPool, query, flags, GetDispatcher());
}

void VulkanCommandBuffer::EndQuery(vk::QueryPool queryPool, uint32_t query)
{
    m_handle.endQuery(queryPool, query, GetDispatcher());
}

void VulkanCommandBuffer::ResetQueryPool(vk::QueryPool queryPool,
                                         uint32_t firstQuery,
                                         uint32_t queryCount)
{
    m_handle.resetQueryPool(queryPool, firstQuery, queryCount, GetDispatcher());
}

void VulkanCommandBuffer::WriteTimestamp(vk::PipelineStageFlagBits pipelineStage,
                                         vk::QueryPool queryPool,
                                         uint32_t query)
{
    m_handle.writeTimestamp(pipelineStage, queryPool, query, GetDispatcher());
}

void VulkanCommandBuffer::CopyQueryPoolResults(
    vk::QueryPool queryPool, uint32_t firstQuery, uint32_t queryCount,
    vk::Buffer dstBuffer, vk::DeviceSize dstOffset, vk::DeviceSize stride,
    vk::QueryResultFlags flags)
{
    m_handle.copyQueryPoolResults(queryPool, firstQuery, queryCount, dstBuffer,
                                  dstOffset, stride, flags, GetDispatcher());
}

void VulkanCommandBuffer::BeginRenderPass(
    const vk::RenderPassBeginInfo& renderPassBegin, vk::SubpassContents contents)
{
    m_handle.beginRenderPass(renderPassBegin, contents, GetDispatcher());
}

void VulkanCommandBuffer::NextSubpass(vk::SubpassContents contents)
{
    m_handle.nextSubpass(contents, GetDispatcher());
}

void VulkanCommandBuffer::EndRenderPass()
{
    m_handle.endRenderPass(GetDispatcher());
}

void VulkanCommandBuffer::PushConstants(
    vk::PipelineLayout layout, vk::ShaderStageFlags stageFlags, uint32_t offset,
    uint32_t size, const void* values)
{
    m_handle.pushConstants(layout, stageFlags, offset, size, values,
                           GetDispatcher());
}

void VulkanCommandBuffer::ExecuteCommands(
    vk::ArrayProxy<const vk::CommandBuffer> commandBuffers)
{
    m_handle.executeCommands(commandBuffers, GetDispatcher());
}

void VulkanCommandBuffer::SetDeviceMask(uint32_t deviceMask)
{
    m_handle.setDeviceMask(deviceMask, GetDispatcher());
}

void VulkanCommandBuffer::DispatchBase(
    uint32_t baseGroupX, uint32_t baseGroupY, uint32_t baseGroupZ,
    uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    m_handle.dispatchBase(baseGroupX, baseGroupY, baseGroupZ, groupCountX,
                          groupCountY, groupCountZ, GetDispatcher());
}

void VulkanCommandBuffer::DrawIndirectCount(
    vk::Buffer buffer, vk::DeviceSize offset, vk::Buffer countBuffer,
    vk::DeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride)
{
    m_handle.drawIndirectCount(buffer, offset, countBuffer, countBufferOffset,
                               maxDrawCount, stride, GetDispatcher());
}

void VulkanCommandBuffer::DrawIndexedIndirectCount(
    vk::Buffer buffer, vk::DeviceSize offset, vk::Buffer countBuffer,
    vk::DeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride)
{
    m_handle.drawIndexedIndirectCount(buffer, offset, countBuffer,
                                      countBufferOffset, maxDrawCount, stride,
                                      GetDispatcher());
}

void VulkanCommandBuffer::BeginRenderPass2(
    const vk::RenderPassBeginInfo& renderPassBegin,
    const vk::SubpassBeginInfo& subpassBeginInfo)
{
    m_handle.beginRenderPass2(renderPassBegin, subpassBeginInfo, GetDispatcher());
}

void VulkanCommandBuffer::NextSubpass2(
    const vk::SubpassBeginInfo& subpassBeginInfo,
    const vk::SubpassEndInfo& subpassEndInfo)
{
    m_handle.nextSubpass2(subpassBeginInfo, subpassEndInfo, GetDispatcher());
}

void VulkanCommandBuffer::EndRenderPass2(
    const vk::SubpassEndInfo& subpassEndInfo)
{
    m_handle.endRenderPass2(subpassEndInfo, GetDispatcher());
}

void VulkanCommandBuffer::PipelineBarrier2(
    const vk::DependencyInfo& dependencyInfo)
{
    m_handle.pipelineBarrier2(dependencyInfo, GetDispatcher());
}

void VulkanCommandBuffer::WriteTimestamp2(vk::PipelineStageFlags2 stage,
                                          vk::QueryPool queryPool,
                                          uint32_t query)
{
    m_handle.writeTimestamp2(stage, queryPool, query, GetDispatcher());
}

void VulkanCommandBuffer::CopyBuffer2(
    const vk::CopyBufferInfo2& copyBufferInfo)
{
    m_handle.copyBuffer2(copyBufferInfo, GetDispatcher());
}

void VulkanCommandBuffer::CopyImage2(const vk::CopyImageInfo2& copyImageInfo)
{
    m_handle.copyImage2(copyImageInfo, GetDispatcher());
}

void VulkanCommandBuffer::CopyBufferToImage2(
    const vk::CopyBufferToImageInfo2& copyBufferToImageInfo)
{
    m_handle.copyBufferToImage2(copyBufferToImageInfo, GetDispatcher());
}

void VulkanCommandBuffer::CopyImageToBuffer2(
    const vk::CopyImageToBufferInfo2& copyImageToBufferInfo)
{
    m_handle.copyImageToBuffer2(copyImageToBufferInfo, GetDispatcher());
}

void VulkanCommandBuffer::BlitImage2(const vk::BlitImageInfo2& blitImageInfo)
{
    m_handle.blitImage2(blitImageInfo, GetDispatcher());
}

void VulkanCommandBuffer::ResolveImage2(
    const vk::ResolveImageInfo2& resolveImageInfo)
{
    m_handle.resolveImage2(resolveImageInfo, GetDispatcher());
}

void VulkanCommandBuffer::SetEvent2(
    vk::Event event, const vk::DependencyInfo& dependencyInfo)
{
    m_handle.setEvent2(event, dependencyInfo, GetDispatcher());
}

void VulkanCommandBuffer::ResetEvent2(vk::Event event,
                                      vk::PipelineStageFlags2 stageMask)
{
    m_handle.resetEvent2(event, stageMask, GetDispatcher());
}

void VulkanCommandBuffer::WaitEvents2(
    vk::ArrayProxy<const vk::Event> events,
    vk::ArrayProxy<const vk::DependencyInfo> dependencyInfos)
{
    m_handle.waitEvents2(events, dependencyInfos, GetDispatcher());
}

void VulkanCommandBuffer::BeginRendering(const vk::RenderingInfo& renderingInfo)
{
    m_handle.beginRendering(renderingInfo, GetDispatcher());
}

void VulkanCommandBuffer::BeginRendering(
    rad::Span<const VulkanRenderingAttachmentInfo> colorAttachments,
    const VulkanRenderingAttachmentInfo* depthAttachment,
    const VulkanRenderingAttachmentInfo* stencilAttachment,
    const vk::Rect2D* renderArea, uint32_t layerCount, uint32_t viewMask,
    vk::RenderingFlags flags)
{
    const auto makeAttachment =
        [](const VulkanRenderingAttachmentInfo& source)
    {
        vk::RenderingAttachmentInfo attachment;
        attachment.imageView =
            source.imageView != nullptr ? source.imageView->GetHandle() : vk::ImageView();
        attachment.imageLayout = source.imageLayout;
        attachment.resolveMode = source.resolveMode;
        attachment.resolveImageView = source.resolveImageView != nullptr
                                          ? source.resolveImageView->GetHandle()
                                          : vk::ImageView();
        attachment.resolveImageLayout = source.resolveImageLayout;
        attachment.loadOp = source.loadOp;
        attachment.storeOp = source.storeOp;
        attachment.clearValue = source.clearValue;
        return attachment;
    };

    rad::SmallVector<vk::RenderingAttachmentInfo, 8> nativeColorAttachments;
    nativeColorAttachments.reserve(colorAttachments.size());
    for (const VulkanRenderingAttachmentInfo& attachment : colorAttachments)
    {
        nativeColorAttachments.push_back(makeAttachment(attachment));
    }

    vk::RenderingAttachmentInfo nativeDepthAttachment;
    const vk::RenderingAttachmentInfo* nativeDepthAttachmentPtr = nullptr;
    if (depthAttachment != nullptr)
    {
        nativeDepthAttachment = makeAttachment(*depthAttachment);
        nativeDepthAttachmentPtr = &nativeDepthAttachment;
    }

    vk::RenderingAttachmentInfo nativeStencilAttachment;
    const vk::RenderingAttachmentInfo* nativeStencilAttachmentPtr = nullptr;
    if (stencilAttachment != nullptr)
    {
        nativeStencilAttachment = makeAttachment(*stencilAttachment);
        nativeStencilAttachmentPtr = &nativeStencilAttachment;
    }

    vk::Rect2D resolvedRenderArea;
    if (renderArea != nullptr)
    {
        resolvedRenderArea = *renderArea;
    }
    else
    {
        const VulkanImageView* extentView = nullptr;
        for (const VulkanRenderingAttachmentInfo& attachment : colorAttachments)
        {
            if (attachment.imageView != nullptr)
            {
                extentView = attachment.imageView;
                break;
            }
        }
        if (extentView == nullptr && depthAttachment != nullptr)
        {
            extentView = depthAttachment->imageView;
        }
        if (extentView == nullptr && stencilAttachment != nullptr)
        {
            extentView = stencilAttachment->imageView;
        }
        if (extentView == nullptr)
        {
            throw std::invalid_argument(
                "BeginRendering requires an attachment or explicit render area");
        }

        vk::Extent3D extent = extentView->GetImage()->GetExtent();
        uint32_t mipLevel =
            extentView->GetSubresourceRange().baseMipLevel;
        while (mipLevel-- > 0)
        {
            extent.width = extent.width > 1 ? extent.width / 2 : 1;
            extent.height = extent.height > 1 ? extent.height / 2 : 1;
        }
        resolvedRenderArea.extent = vk::Extent2D{extent.width, extent.height};
    }

    vk::RenderingInfo renderingInfo;
    renderingInfo.flags = flags;
    renderingInfo.renderArea = resolvedRenderArea;
    renderingInfo.layerCount = layerCount;
    renderingInfo.viewMask = viewMask;
    renderingInfo.colorAttachmentCount =
        static_cast<uint32_t>(nativeColorAttachments.size());
    renderingInfo.pColorAttachments = nativeColorAttachments.data();
    renderingInfo.pDepthAttachment = nativeDepthAttachmentPtr;
    renderingInfo.pStencilAttachment = nativeStencilAttachmentPtr;
    BeginRendering(renderingInfo);
}

void VulkanCommandBuffer::EndRendering()
{
    m_handle.endRendering(GetDispatcher());
}

void VulkanCommandBuffer::SetCullMode(vk::CullModeFlags cullMode)
{
    m_handle.setCullMode(cullMode, GetDispatcher());
}

void VulkanCommandBuffer::SetFrontFace(vk::FrontFace frontFace)
{
    m_handle.setFrontFace(frontFace, GetDispatcher());
}

void VulkanCommandBuffer::SetPrimitiveTopology(
    vk::PrimitiveTopology primitiveTopology)
{
    m_handle.setPrimitiveTopology(primitiveTopology, GetDispatcher());
}

void VulkanCommandBuffer::SetViewportWithCount(
    vk::ArrayProxy<const vk::Viewport> viewports)
{
    m_handle.setViewportWithCount(viewports, GetDispatcher());
}

void VulkanCommandBuffer::SetScissorWithCount(
    vk::ArrayProxy<const vk::Rect2D> scissors)
{
    m_handle.setScissorWithCount(scissors, GetDispatcher());
}

void VulkanCommandBuffer::BindVertexBuffers2(
    uint32_t firstBinding, vk::ArrayProxy<const vk::Buffer> buffers,
    vk::ArrayProxy<const vk::DeviceSize> offsets,
    vk::ArrayProxy<const vk::DeviceSize> sizes,
    vk::ArrayProxy<const vk::DeviceSize> strides)
{
    m_handle.bindVertexBuffers2(firstBinding, buffers, offsets, sizes, strides,
                                GetDispatcher());
}

void VulkanCommandBuffer::SetDepthTestEnable(vk::Bool32 depthTestEnable)
{
    m_handle.setDepthTestEnable(depthTestEnable, GetDispatcher());
}

void VulkanCommandBuffer::SetDepthWriteEnable(vk::Bool32 depthWriteEnable)
{
    m_handle.setDepthWriteEnable(depthWriteEnable, GetDispatcher());
}

void VulkanCommandBuffer::SetDepthCompareOp(vk::CompareOp depthCompareOp)
{
    m_handle.setDepthCompareOp(depthCompareOp, GetDispatcher());
}

void VulkanCommandBuffer::SetDepthBoundsTestEnable(
    vk::Bool32 depthBoundsTestEnable)
{
    m_handle.setDepthBoundsTestEnable(depthBoundsTestEnable, GetDispatcher());
}

void VulkanCommandBuffer::SetStencilTestEnable(vk::Bool32 stencilTestEnable)
{
    m_handle.setStencilTestEnable(stencilTestEnable, GetDispatcher());
}

void VulkanCommandBuffer::SetStencilOp(
    vk::StencilFaceFlags faceMask, vk::StencilOp failOp, vk::StencilOp passOp,
    vk::StencilOp depthFailOp, vk::CompareOp compareOp)
{
    m_handle.setStencilOp(faceMask, failOp, passOp, depthFailOp, compareOp,
                          GetDispatcher());
}

void VulkanCommandBuffer::SetRasterizerDiscardEnable(
    vk::Bool32 rasterizerDiscardEnable)
{
    m_handle.setRasterizerDiscardEnable(rasterizerDiscardEnable, GetDispatcher());
}

void VulkanCommandBuffer::SetDepthBiasEnable(vk::Bool32 depthBiasEnable)
{
    m_handle.setDepthBiasEnable(depthBiasEnable, GetDispatcher());
}

void VulkanCommandBuffer::SetPrimitiveRestartEnable(
    vk::Bool32 primitiveRestartEnable)
{
    m_handle.setPrimitiveRestartEnable(primitiveRestartEnable, GetDispatcher());
}

void VulkanCommandBuffer::PipelineBarrier(const vk::DependencyInfo& dependencyInfo)
{
    PipelineBarrier2(dependencyInfo);
}

void VulkanCommandBuffer::PipelineBarrier(
    vk::ArrayProxy<const vk::MemoryBarrier2> memoryBarriers,
    vk::ArrayProxy<const vk::BufferMemoryBarrier2> bufferMemoryBarriers,
    vk::ArrayProxy<const vk::ImageMemoryBarrier2> imageMemoryBarriers,
    vk::DependencyFlags dependencyFlags)
{
    vk::DependencyInfo dependencyInfo;
    dependencyInfo.dependencyFlags = dependencyFlags;
    dependencyInfo.memoryBarrierCount = memoryBarriers.size();
    dependencyInfo.pMemoryBarriers = memoryBarriers.data();
    dependencyInfo.bufferMemoryBarrierCount = bufferMemoryBarriers.size();
    dependencyInfo.pBufferMemoryBarriers = bufferMemoryBarriers.data();
    dependencyInfo.imageMemoryBarrierCount = imageMemoryBarriers.size();
    dependencyInfo.pImageMemoryBarriers = imageMemoryBarriers.data();
    PipelineBarrier(dependencyInfo);
}

void VulkanCommandBuffer::ExecutionBarrier(
    vk::PipelineStageFlags2 srcStageMask, vk::PipelineStageFlags2 dstStageMask,
    vk::DependencyFlags dependencyFlags)
{
    MemoryBarrier(srcStageMask, {}, dstStageMask, {}, dependencyFlags);
}

void VulkanCommandBuffer::MemoryBarrier(
    vk::PipelineStageFlags2 srcStageMask, vk::AccessFlags2 srcAccessMask,
    vk::PipelineStageFlags2 dstStageMask, vk::AccessFlags2 dstAccessMask,
    vk::DependencyFlags dependencyFlags)
{
    vk::MemoryBarrier2 memoryBarrier;
    memoryBarrier.srcStageMask = srcStageMask;
    memoryBarrier.srcAccessMask = srcAccessMask;
    memoryBarrier.dstStageMask = dstStageMask;
    memoryBarrier.dstAccessMask = dstAccessMask;
    PipelineBarrier(memoryBarrier, {}, {}, dependencyFlags);
}

void VulkanCommandBuffer::BufferMemoryBarrier(
    vk::Buffer buffer, vk::PipelineStageFlags2 srcStageMask,
    vk::AccessFlags2 srcAccessMask, vk::PipelineStageFlags2 dstStageMask,
    vk::AccessFlags2 dstAccessMask, vk::DeviceSize offset, vk::DeviceSize size,
    uint32_t srcQueueFamilyIndex, uint32_t dstQueueFamilyIndex,
    vk::DependencyFlags dependencyFlags)
{
    vk::BufferMemoryBarrier2 bufferMemoryBarrier;
    bufferMemoryBarrier.srcStageMask = srcStageMask;
    bufferMemoryBarrier.srcAccessMask = srcAccessMask;
    bufferMemoryBarrier.dstStageMask = dstStageMask;
    bufferMemoryBarrier.dstAccessMask = dstAccessMask;
    bufferMemoryBarrier.srcQueueFamilyIndex = srcQueueFamilyIndex;
    bufferMemoryBarrier.dstQueueFamilyIndex = dstQueueFamilyIndex;
    bufferMemoryBarrier.buffer = buffer;
    bufferMemoryBarrier.offset = offset;
    bufferMemoryBarrier.size = size;
    PipelineBarrier({}, bufferMemoryBarrier, {}, dependencyFlags);
}

void VulkanCommandBuffer::ImageMemoryBarrier(
    vk::Image image, const vk::ImageSubresourceRange& subresourceRange,
    vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
    vk::PipelineStageFlags2 srcStageMask, vk::AccessFlags2 srcAccessMask,
    vk::PipelineStageFlags2 dstStageMask, vk::AccessFlags2 dstAccessMask,
    uint32_t srcQueueFamilyIndex, uint32_t dstQueueFamilyIndex,
    vk::DependencyFlags dependencyFlags)
{
    vk::ImageMemoryBarrier2 imageMemoryBarrier;
    imageMemoryBarrier.srcStageMask = srcStageMask;
    imageMemoryBarrier.srcAccessMask = srcAccessMask;
    imageMemoryBarrier.dstStageMask = dstStageMask;
    imageMemoryBarrier.dstAccessMask = dstAccessMask;
    imageMemoryBarrier.oldLayout = oldLayout;
    imageMemoryBarrier.newLayout = newLayout;
    imageMemoryBarrier.srcQueueFamilyIndex = srcQueueFamilyIndex;
    imageMemoryBarrier.dstQueueFamilyIndex = dstQueueFamilyIndex;
    imageMemoryBarrier.image = image;
    imageMemoryBarrier.subresourceRange = subresourceRange;
    PipelineBarrier({}, {}, imageMemoryBarrier, dependencyFlags);
}

} // namespace sd
