#pragma once

#include <SweetDream/GPU/Vulkan/VulkanCommon.h>

namespace sd
{

class VulkanBuffer;
class VulkanCommandBuffer;
class VulkanDevice;
class VulkanImage;

class VulkanImageReadback
{
public:
    struct Layout
    {
        vk::Format format = vk::Format::eUndefined;
        vk::Extent2D extent = {};
        std::size_t rowBytes = 0;
        std::size_t sizeBytes = 0;
    };

    explicit VulkanImageReadback(VulkanDevice* device);
    ~VulkanImageReadback();

    VulkanImageReadback(const VulkanImageReadback&) = delete;
    VulkanImageReadback& operator=(const VulkanImageReadback&) = delete;

    // Record outside rendering on a transfer-capable queue with synchronization2.
    // Copies mip 0, layer 0 of an initialized single-sample 2D color image with
    // transfer-source usage. Supports RGBA8 and BGRA8 UNORM/sRGB, and RGBA32F;
    // bytes are tightly packed in the source format, without conversion.
    // No submission, wait, or queue ownership transfer is performed. Keep the
    // source and this object alive until completion; only one copy may be in flight.
    [[nodiscard]] Layout Execute(VulkanCommandBuffer* command, VulkanImage* source,
                                 VulkanImageState before, VulkanImageState after);

    // Same lifetime and synchronization contract as Execute, but always returns
    // RGBA32F. RGBA32F sources are copied directly; RGBA16F and R11G11B10 UFLOAT
    // require a graphics queue and format blit support. The source must contain
    // linear RGB values. No PQ decoding, gamut conversion, or tone mapping.
    [[nodiscard]] Layout ExecuteHDR(VulkanCommandBuffer* command, VulkanImage* source,
                                    VulkanImageState before, VulkanImageState after);

    // After waiting for the submission recorded by Execute, copy its entire
    // layout.sizeBytes to destination. capacityBytes may be larger.
    void ReadHost(void* destination, std::size_t capacityBytes);

    // Like ReadHost, call after the submission completes. Neither method waits.
    // PNG accepts RGBA8/BGRA8 UNORM or sRGB; it preserves alpha and does not
    // convert color spaces. HDR accepts RGBA32F and writes RGBE without alpha
    // or tone mapping. An incompatible format throws; encoder failures return false.
    [[nodiscard]] bool SavePNG(const std::string& fileName);
    [[nodiscard]] bool SaveHDR(const std::string& fileName);

private:
    [[nodiscard]] Layout ExecuteImpl(VulkanCommandBuffer* command, VulkanImage* source,
                                     VulkanImageState before, VulkanImageState after, bool hdr);
    void RequireRecordedCopy() const;

    rad::Ref<VulkanDevice> m_device;
    rad::Ref<VulkanBuffer> m_buffer;
    rad::Ref<VulkanImage> m_hdrImage;
    Layout m_layout;
}; // class VulkanImageReadback

} // namespace sd
