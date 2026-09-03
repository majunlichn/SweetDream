#pragma once

#include <SweetDream/Core/API/Vulkan/VulkanDevice.h>

#include <rad/Core/Span.h>

#include <array>

namespace sd
{

class VulkanCommandBuffer;
class VulkanImageView;

struct VulkanRenderingAttachmentInfo
{
    VulkanImageView* imageView = nullptr;
    vk::ImageLayout imageLayout = vk::ImageLayout::eAttachmentOptimal;
    vk::AttachmentLoadOp loadOp = vk::AttachmentLoadOp::eClear;
    vk::AttachmentStoreOp storeOp = vk::AttachmentStoreOp::eStore;
    vk::ClearValue clearValue = {};
    vk::ResolveModeFlagBits resolveMode = vk::ResolveModeFlagBits::eNone;
    VulkanImageView* resolveImageView = nullptr;
    vk::ImageLayout resolveImageLayout = vk::ImageLayout::eAttachmentOptimal;
};

class VulkanCommandPool : public rad::RefCounted<VulkanCommandPool>
{
public:
    VulkanCommandPool(rad::Ref<VulkanDevice> device, VulkanQueueFamily queueFamily,
                      vk::CommandPoolCreateFlags flags = {});
    ~VulkanCommandPool();

    VulkanCommandPool(const VulkanCommandPool&) = delete;
    VulkanCommandPool& operator=(const VulkanCommandPool&) = delete;

    [[nodiscard]] VulkanDevice* GetDevice() const noexcept { return m_device.get(); }
    [[nodiscard]] const vk::detail::DispatchLoaderDynamic& GetDispatcher() const noexcept;
    [[nodiscard]] const vk::CommandPool& GetHandle() const noexcept { return m_handle; }
    [[nodiscard]] VulkanQueueFamily GetQueueFamily() const noexcept { return m_queueFamily; }

    [[nodiscard]] std::vector<rad::Ref<VulkanCommandBuffer>> AllocateCommandBuffers(
        vk::CommandBufferLevel level, uint32_t count);
    [[nodiscard]] rad::Ref<VulkanCommandBuffer> AllocateCommandBuffer(
        vk::CommandBufferLevel level);

private:
    rad::Ref<VulkanDevice> m_device;
    VulkanQueueFamily m_queueFamily;
    vk::CommandPool m_handle = nullptr;
}; // class VulkanCommandPool

class VulkanCommandBuffer : public rad::RefCounted<VulkanCommandBuffer>
{
public:
    ~VulkanCommandBuffer();

    VulkanCommandBuffer(const VulkanCommandBuffer&) = delete;
    VulkanCommandBuffer& operator=(const VulkanCommandBuffer&) = delete;

    [[nodiscard]] VulkanDevice* GetDevice() const noexcept
    {
        return m_cmdPool->GetDevice();
    }
    [[nodiscard]] const vk::detail::DispatchLoaderDynamic& GetDispatcher() const noexcept;
    [[nodiscard]] VulkanCommandPool* GetCommandPool() const noexcept
    {
        return m_cmdPool.get();
    }
    [[nodiscard]] const vk::CommandBuffer& GetHandle() const noexcept { return m_handle; }

    void Begin(vk::CommandBufferUsageFlags flags =
                   vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
               const vk::CommandBufferInheritanceInfo* pInheritanceInfo = nullptr);
    void End();
    void Reset(vk::CommandBufferResetFlags flags = {});

    // Pipeline, resource, and dynamic-state binding.
    void BindPipeline(vk::PipelineBindPoint pipelineBindPoint, vk::Pipeline pipeline);
    void BindDescriptorSets(
        vk::PipelineBindPoint pipelineBindPoint, vk::PipelineLayout layout,
        uint32_t firstSet, vk::ArrayProxy<const vk::DescriptorSet> descriptorSets,
        vk::ArrayProxy<const uint32_t> dynamicOffsets = {});
    void BindIndexBuffer(vk::Buffer buffer, vk::DeviceSize offset,
                         vk::IndexType indexType);
    void BindVertexBuffers(uint32_t firstBinding,
                           vk::ArrayProxy<const vk::Buffer> buffers,
                           vk::ArrayProxy<const vk::DeviceSize> offsets);

    void SetViewport(uint32_t firstViewport,
                     vk::ArrayProxy<const vk::Viewport> viewports);
    void SetScissor(uint32_t firstScissor, vk::ArrayProxy<const vk::Rect2D> scissors);
    void SetLineWidth(float lineWidth);
    void SetDepthBias(float depthBiasConstantFactor, float depthBiasClamp,
                      float depthBiasSlopeFactor);
    void SetBlendConstants(const std::array<float, 4>& blendConstants);
    void SetDepthBounds(float minDepthBounds, float maxDepthBounds);
    void SetStencilCompareMask(vk::StencilFaceFlags faceMask, uint32_t compareMask);
    void SetStencilWriteMask(vk::StencilFaceFlags faceMask, uint32_t writeMask);
    void SetStencilReference(vk::StencilFaceFlags faceMask, uint32_t reference);

    // Drawing and dispatch.
    void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex,
              uint32_t firstInstance);
    void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex,
                     int32_t vertexOffset, uint32_t firstInstance);
    void DrawIndirect(vk::Buffer buffer, vk::DeviceSize offset, uint32_t drawCount,
                      uint32_t stride);
    void DrawIndexedIndirect(vk::Buffer buffer, vk::DeviceSize offset,
                             uint32_t drawCount, uint32_t stride);
    void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
    void DispatchIndirect(vk::Buffer buffer, vk::DeviceSize offset);

    // Transfer, update, clear, and resolve.
    void CopyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer,
                    vk::ArrayProxy<const vk::BufferCopy> regions);
    void CopyImage(vk::Image srcImage, vk::ImageLayout srcImageLayout,
                   vk::Image dstImage, vk::ImageLayout dstImageLayout,
                   vk::ArrayProxy<const vk::ImageCopy> regions);
    void BlitImage(vk::Image srcImage, vk::ImageLayout srcImageLayout,
                   vk::Image dstImage, vk::ImageLayout dstImageLayout,
                   vk::ArrayProxy<const vk::ImageBlit> regions, vk::Filter filter);
    void CopyBufferToImage(vk::Buffer srcBuffer, vk::Image dstImage,
                           vk::ImageLayout dstImageLayout,
                           vk::ArrayProxy<const vk::BufferImageCopy> regions);
    void CopyImageToBuffer(vk::Image srcImage, vk::ImageLayout srcImageLayout,
                           vk::Buffer dstBuffer,
                           vk::ArrayProxy<const vk::BufferImageCopy> regions);
    void UpdateBuffer(vk::Buffer dstBuffer, vk::DeviceSize dstOffset,
                      vk::DeviceSize dataSize, const void* data);
    void FillBuffer(vk::Buffer dstBuffer, vk::DeviceSize dstOffset,
                    vk::DeviceSize size, uint32_t data);
    void ClearColorImage(
        vk::Image image, vk::ImageLayout imageLayout,
        const vk::ClearColorValue& color,
        vk::ArrayProxy<const vk::ImageSubresourceRange> ranges);
    void ClearDepthStencilImage(
        vk::Image image, vk::ImageLayout imageLayout,
        const vk::ClearDepthStencilValue& depthStencil,
        vk::ArrayProxy<const vk::ImageSubresourceRange> ranges);
    void ClearAttachments(vk::ArrayProxy<const vk::ClearAttachment> attachments,
                          vk::ArrayProxy<const vk::ClearRect> rects);
    void ResolveImage(vk::Image srcImage, vk::ImageLayout srcImageLayout,
                      vk::Image dstImage, vk::ImageLayout dstImageLayout,
                      vk::ArrayProxy<const vk::ImageResolve> regions);

    // Legacy synchronization and events.
    void PipelineBarrier(
        vk::PipelineStageFlags srcStageMask, vk::PipelineStageFlags dstStageMask,
        vk::DependencyFlags dependencyFlags,
        vk::ArrayProxy<const vk::MemoryBarrier> memoryBarriers = {},
        vk::ArrayProxy<const vk::BufferMemoryBarrier> bufferMemoryBarriers = {},
        vk::ArrayProxy<const vk::ImageMemoryBarrier> imageMemoryBarriers = {});
    void SetEvent(vk::Event event, vk::PipelineStageFlags stageMask);
    void ResetEvent(vk::Event event, vk::PipelineStageFlags stageMask);
    void WaitEvents(
        vk::ArrayProxy<const vk::Event> events,
        vk::PipelineStageFlags srcStageMask, vk::PipelineStageFlags dstStageMask,
        vk::ArrayProxy<const vk::MemoryBarrier> memoryBarriers = {},
        vk::ArrayProxy<const vk::BufferMemoryBarrier> bufferMemoryBarriers = {},
        vk::ArrayProxy<const vk::ImageMemoryBarrier> imageMemoryBarriers = {});

    // Queries, render passes, constants, and secondary command buffers.
    void BeginQuery(vk::QueryPool queryPool, uint32_t query,
                    vk::QueryControlFlags flags = {});
    void EndQuery(vk::QueryPool queryPool, uint32_t query);
    void ResetQueryPool(vk::QueryPool queryPool, uint32_t firstQuery,
                        uint32_t queryCount);
    void WriteTimestamp(vk::PipelineStageFlagBits pipelineStage,
                        vk::QueryPool queryPool, uint32_t query);
    void CopyQueryPoolResults(vk::QueryPool queryPool, uint32_t firstQuery,
                              uint32_t queryCount, vk::Buffer dstBuffer,
                              vk::DeviceSize dstOffset, vk::DeviceSize stride,
                              vk::QueryResultFlags flags = {});
    void BeginRenderPass(const vk::RenderPassBeginInfo& renderPassBegin,
                         vk::SubpassContents contents);
    void NextSubpass(vk::SubpassContents contents);
    void EndRenderPass();
    void PushConstants(vk::PipelineLayout layout, vk::ShaderStageFlags stageFlags,
                       uint32_t offset, uint32_t size, const void* values);
    void ExecuteCommands(vk::ArrayProxy<const vk::CommandBuffer> commandBuffers);

    // Vulkan 1.1.
    void SetDeviceMask(uint32_t deviceMask);
    void DispatchBase(uint32_t baseGroupX, uint32_t baseGroupY,
                      uint32_t baseGroupZ, uint32_t groupCountX,
                      uint32_t groupCountY, uint32_t groupCountZ);

    // Vulkan 1.2.
    void DrawIndirectCount(vk::Buffer buffer, vk::DeviceSize offset,
                           vk::Buffer countBuffer,
                           vk::DeviceSize countBufferOffset,
                           uint32_t maxDrawCount, uint32_t stride);
    void DrawIndexedIndirectCount(vk::Buffer buffer, vk::DeviceSize offset,
                                  vk::Buffer countBuffer,
                                  vk::DeviceSize countBufferOffset,
                                  uint32_t maxDrawCount, uint32_t stride);
    void BeginRenderPass2(const vk::RenderPassBeginInfo& renderPassBegin,
                          const vk::SubpassBeginInfo& subpassBeginInfo);
    void NextSubpass2(const vk::SubpassBeginInfo& subpassBeginInfo,
                      const vk::SubpassEndInfo& subpassEndInfo);
    void EndRenderPass2(const vk::SubpassEndInfo& subpassEndInfo);

    // Vulkan 1.3 synchronization, transfer, rendering, and dynamic state.
    void PipelineBarrier2(const vk::DependencyInfo& dependencyInfo);
    void WriteTimestamp2(vk::PipelineStageFlags2 stage, vk::QueryPool queryPool,
                         uint32_t query);
    void CopyBuffer2(const vk::CopyBufferInfo2& copyBufferInfo);
    void CopyImage2(const vk::CopyImageInfo2& copyImageInfo);
    void CopyBufferToImage2(const vk::CopyBufferToImageInfo2& copyBufferToImageInfo);
    void CopyImageToBuffer2(const vk::CopyImageToBufferInfo2& copyImageToBufferInfo);
    void BlitImage2(const vk::BlitImageInfo2& blitImageInfo);
    void ResolveImage2(const vk::ResolveImageInfo2& resolveImageInfo);
    void SetEvent2(vk::Event event, const vk::DependencyInfo& dependencyInfo);
    void ResetEvent2(vk::Event event, vk::PipelineStageFlags2 stageMask);
    void WaitEvents2(vk::ArrayProxy<const vk::Event> events,
                     vk::ArrayProxy<const vk::DependencyInfo> dependencyInfos);
    void BeginRendering(const vk::RenderingInfo& renderingInfo);
    void BeginRendering(
        rad::Span<const VulkanRenderingAttachmentInfo> colorAttachments,
        const VulkanRenderingAttachmentInfo* depthAttachment = nullptr,
        const VulkanRenderingAttachmentInfo* stencilAttachment = nullptr,
        const vk::Rect2D* renderArea = nullptr, uint32_t layerCount = 1,
        uint32_t viewMask = 0, vk::RenderingFlags flags = {});
    void EndRendering();
    void SetCullMode(vk::CullModeFlags cullMode);
    void SetFrontFace(vk::FrontFace frontFace);
    void SetPrimitiveTopology(vk::PrimitiveTopology primitiveTopology);
    void SetViewportWithCount(vk::ArrayProxy<const vk::Viewport> viewports);
    void SetScissorWithCount(vk::ArrayProxy<const vk::Rect2D> scissors);
    void BindVertexBuffers2(
        uint32_t firstBinding, vk::ArrayProxy<const vk::Buffer> buffers,
        vk::ArrayProxy<const vk::DeviceSize> offsets,
        vk::ArrayProxy<const vk::DeviceSize> sizes = {},
        vk::ArrayProxy<const vk::DeviceSize> strides = {});
    void SetDepthTestEnable(vk::Bool32 depthTestEnable);
    void SetDepthWriteEnable(vk::Bool32 depthWriteEnable);
    void SetDepthCompareOp(vk::CompareOp depthCompareOp);
    void SetDepthBoundsTestEnable(vk::Bool32 depthBoundsTestEnable);
    void SetStencilTestEnable(vk::Bool32 stencilTestEnable);
    void SetStencilOp(vk::StencilFaceFlags faceMask, vk::StencilOp failOp,
                      vk::StencilOp passOp, vk::StencilOp depthFailOp,
                      vk::CompareOp compareOp);
    void SetRasterizerDiscardEnable(vk::Bool32 rasterizerDiscardEnable);
    void SetDepthBiasEnable(vk::Bool32 depthBiasEnable);
    void SetPrimitiveRestartEnable(vk::Bool32 primitiveRestartEnable);

    // Synchronization2 convenience helpers.
    void PipelineBarrier(const vk::DependencyInfo& dependencyInfo);
    void PipelineBarrier(
        vk::ArrayProxy<const vk::MemoryBarrier2> memoryBarriers,
        vk::ArrayProxy<const vk::BufferMemoryBarrier2> bufferMemoryBarriers = {},
        vk::ArrayProxy<const vk::ImageMemoryBarrier2> imageMemoryBarriers = {},
        vk::DependencyFlags dependencyFlags = {});

    void ExecutionBarrier(vk::PipelineStageFlags2 srcStageMask,
                          vk::PipelineStageFlags2 dstStageMask,
                          vk::DependencyFlags dependencyFlags = {});
    void MemoryBarrier(vk::PipelineStageFlags2 srcStageMask,
                       vk::AccessFlags2 srcAccessMask,
                       vk::PipelineStageFlags2 dstStageMask,
                       vk::AccessFlags2 dstAccessMask,
                       vk::DependencyFlags dependencyFlags = {});
    void BufferMemoryBarrier(
        vk::Buffer buffer, vk::PipelineStageFlags2 srcStageMask,
        vk::AccessFlags2 srcAccessMask, vk::PipelineStageFlags2 dstStageMask,
        vk::AccessFlags2 dstAccessMask, vk::DeviceSize offset = 0,
        vk::DeviceSize size = VK_WHOLE_SIZE,
        uint32_t srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        uint32_t dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        vk::DependencyFlags dependencyFlags = {});
    void ImageMemoryBarrier(
        vk::Image image, const vk::ImageSubresourceRange& subresourceRange,
        vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
        vk::PipelineStageFlags2 srcStageMask, vk::AccessFlags2 srcAccessMask,
        vk::PipelineStageFlags2 dstStageMask, vk::AccessFlags2 dstAccessMask,
        uint32_t srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        uint32_t dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        vk::DependencyFlags dependencyFlags = {});

private:
    friend class VulkanCommandPool;

    VulkanCommandBuffer(rad::Ref<VulkanCommandPool> cmdPool,
                        vk::CommandBuffer cmdBufferHandle);

    rad::Ref<VulkanCommandPool> m_cmdPool;
    vk::CommandBuffer m_handle = nullptr;
}; // class VulkanCommandBuffer

} // namespace sd
