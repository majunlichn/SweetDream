#pragma once

#include <SweetDream/Core/API/Vulkan/VulkanCommon.h>

namespace sd
{

class VulkanDevice;
class VulkanImageView;

class VulkanImage : public rad::RefCounted<VulkanImage>
{
public:
    VulkanImage(rad::Ref<VulkanDevice> device, const vk::ImageCreateInfo& createInfo,
                const VmaAllocationCreateInfo& allocationCreateInfo);
    VulkanImage(rad::Ref<VulkanDevice> device, const vk::ImageCreateInfo& createInfo,
                vk::Image handle);
    ~VulkanImage();

    VulkanImage(const VulkanImage&) = delete;
    VulkanImage& operator=(const VulkanImage&) = delete;

    static rad::Ref<VulkanImage> Create(
        rad::Ref<VulkanDevice> device, const vk::ImageCreateInfo& createInfo,
        const VmaAllocationCreateInfo& allocationCreateInfo);
    static rad::Ref<VulkanImage> WrapBorrowed(
        rad::Ref<VulkanDevice> device, const vk::ImageCreateInfo& createInfo,
        vk::Image handle);

    [[nodiscard]] VulkanDevice* GetDevice() const noexcept { return m_device.get(); }
    [[nodiscard]] const vk::detail::DispatchLoaderDynamic& GetDispatcher() const noexcept;
    [[nodiscard]] vk::Image GetHandle() const noexcept { return m_handle; }

    [[nodiscard]] vk::ImageCreateFlags GetFlags() const noexcept { return m_flags; }
    [[nodiscard]] vk::ImageType GetImageType() const noexcept { return m_imageType; }
    [[nodiscard]] vk::Format GetFormat() const noexcept { return m_format; }
    [[nodiscard]] vk::Extent3D GetExtent() const noexcept { return m_extent; }
    [[nodiscard]] uint32_t GetWidth() const noexcept { return m_extent.width; }
    [[nodiscard]] uint32_t GetHeight() const noexcept { return m_extent.height; }
    [[nodiscard]] uint32_t GetDepth() const noexcept { return m_extent.depth; }
    [[nodiscard]] uint32_t GetMipLevels() const noexcept { return m_mipLevels; }
    [[nodiscard]] uint32_t GetArrayLayers() const noexcept { return m_arrayLayers; }
    [[nodiscard]] vk::SampleCountFlagBits GetSamples() const noexcept { return m_samples; }
    [[nodiscard]] vk::ImageTiling GetTiling() const noexcept { return m_tiling; }
    [[nodiscard]] vk::ImageUsageFlags GetUsage() const noexcept { return m_usage; }
    [[nodiscard]] vk::SharingMode GetSharingMode() const noexcept
    {
        return m_sharingMode;
    }

    [[nodiscard]] bool IsHostVisible() const noexcept
    {
        return HasAnyBits(m_memPropFlags, vk::MemoryPropertyFlagBits::eHostVisible);
    }
    [[nodiscard]] bool IsHostCoherent() const noexcept
    {
        return HasAnyBits(m_memPropFlags, vk::MemoryPropertyFlagBits::eHostCoherent);
    }

    rad::Ref<VulkanImageView> CreateView(
        vk::ImageViewType type, vk::Format format,
        const vk::ImageSubresourceRange& range,
        vk::ComponentMapping components = vk::ComponentMapping());
    rad::Ref<VulkanImageView> CreateView(vk::ImageViewType type, vk::Format format);
    rad::Ref<VulkanImageView> CreateView(vk::ImageViewType type);
    rad::Ref<VulkanImageView> CreateView2D(uint32_t baseMipLevel = 0,
                                           uint32_t levelCount = 1,
                                           uint32_t baseArrayLayer = 0);

private:
    rad::Ref<VulkanDevice> m_device;
    vk::Image m_handle = nullptr;
    VmaAllocation m_alloc = nullptr;
    VmaAllocationInfo m_allocInfo = {};
    vk::MemoryPropertyFlags m_memPropFlags = {};

    vk::ImageCreateFlags m_flags = {};
    vk::ImageType m_imageType = vk::ImageType::e1D;
    vk::Format m_format = vk::Format::eUndefined;
    vk::Extent3D m_extent = {};
    uint32_t m_mipLevels = {};
    uint32_t m_arrayLayers = {};
    vk::SampleCountFlagBits m_samples = vk::SampleCountFlagBits::e1;
    vk::ImageTiling m_tiling = vk::ImageTiling::eOptimal;
    vk::ImageUsageFlags m_usage = {};
    vk::SharingMode m_sharingMode = vk::SharingMode::eExclusive;

}; // class VulkanImage

class VulkanImageView : public rad::RefCounted<VulkanImageView>
{
public:
    VulkanImageView(rad::Ref<VulkanImage> image,
                    const vk::ImageViewCreateInfo& createInfo);
    ~VulkanImageView();

    VulkanImageView(const VulkanImageView&) = delete;
    VulkanImageView& operator=(const VulkanImageView&) = delete;

    [[nodiscard]] VulkanDevice* GetDevice() const noexcept
    {
        return m_image->GetDevice();
    }
    [[nodiscard]] const vk::detail::DispatchLoaderDynamic& GetDispatcher() const noexcept;
    [[nodiscard]] VulkanImage* GetImage() const noexcept { return m_image.get(); }
    [[nodiscard]] vk::ImageView GetHandle() const noexcept { return m_handle; }

    [[nodiscard]] vk::ImageViewType GetType() const noexcept { return m_type; }
    [[nodiscard]] vk::Format GetFormat() const noexcept { return m_format; }
    [[nodiscard]] const vk::ImageSubresourceRange& GetSubresourceRange() const noexcept
    {
        return m_range;
    }
    [[nodiscard]] const vk::ComponentMapping& GetComponentMapping() const noexcept
    {
        return m_components;
    }

private:
    rad::Ref<VulkanImage> m_image;
    vk::ImageView m_handle = nullptr;
    vk::ImageViewType m_type = {};
    vk::Format m_format = vk::Format::eUndefined;
    vk::ImageSubresourceRange m_range = {};
    vk::ComponentMapping m_components = {};
}; // class VulkanImageView

} // namespace sd
