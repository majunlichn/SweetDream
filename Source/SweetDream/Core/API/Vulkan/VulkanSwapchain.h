#pragma once

#include <SweetDream/Core/API/Vulkan/VulkanCommon.h>

namespace sd
{

class VulkanDevice;
class VulkanFence;
class VulkanImage;
class VulkanImageView;
class VulkanSemaphore;
class VulkanSurface;

class VulkanSwapchain : public rad::RefCounted<VulkanSwapchain>
{
public:
    VulkanSwapchain(rad::Ref<VulkanDevice> device, rad::Ref<VulkanSurface> surface,
                    const vk::SwapchainCreateInfoKHR& createInfo);
    // The caller must ensure that all rendering and presentation operations
    // using this swapchain have completed before destruction.
    ~VulkanSwapchain();

    VulkanSwapchain(const VulkanSwapchain&) = delete;
    VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;

    [[nodiscard]] VulkanDevice* GetDevice() const noexcept { return m_device.get(); }
    [[nodiscard]] VulkanSurface* GetSurface() const noexcept { return m_surface.get(); }
    [[nodiscard]] const vk::detail::DispatchLoaderDynamic& GetDispatcher() const noexcept;
    [[nodiscard]] vk::SwapchainKHR GetHandle() const noexcept { return m_handle; }

    [[nodiscard]] uint32_t GetImageCount() const noexcept { return m_imageCount; }
    [[nodiscard]] vk::Format GetImageFormat() const noexcept { return m_imageFormat; }
    [[nodiscard]] vk::ColorSpaceKHR GetImageColorSpace() const noexcept
    {
        return m_imageColorSpace;
    }
    [[nodiscard]] vk::Extent2D GetImageExtent() const noexcept { return m_imageExtent; }
    [[nodiscard]] uint32_t GetImageWidth() const noexcept { return m_imageExtent.width; }
    [[nodiscard]] uint32_t GetImageHeight() const noexcept { return m_imageExtent.height; }
    [[nodiscard]] uint32_t GetImageArrayLayers() const noexcept
    {
        return m_imageArrayLayers;
    }
    [[nodiscard]] vk::ImageUsageFlags GetImageUsage() const noexcept
    {
        return m_imageUsage;
    }
    [[nodiscard]] vk::SurfaceTransformFlagBitsKHR GetPreTransform() const noexcept
    {
        return m_preTransform;
    }
    [[nodiscard]] vk::CompositeAlphaFlagBitsKHR GetCompositeAlpha() const noexcept
    {
        return m_compositeAlpha;
    }
    [[nodiscard]] vk::PresentModeKHR GetPresentMode() const noexcept
    {
        return m_presentMode;
    }

    [[nodiscard]] VulkanImage* GetImage(uint32_t index) const
    {
        return m_images.at(index).get();
    }
    [[nodiscard]] VulkanImageView* GetImageView(uint32_t index) const
    {
        return m_imageViews.at(index).get();
    }

    [[nodiscard]] std::pair<vk::Result, uint32_t> AcquireNextImage(
        uint64_t timeout, VulkanSemaphore* semaphore, VulkanFence* fence,
        uint32_t deviceMask = 1);

private:
    rad::Ref<VulkanDevice> m_device;
    rad::Ref<VulkanSurface> m_surface;
    vk::SwapchainKHR m_handle = nullptr;

    uint32_t m_imageCount = 0;
    vk::Format m_imageFormat = vk::Format::eUndefined;
    vk::ColorSpaceKHR m_imageColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
    vk::Extent2D m_imageExtent = {};
    uint32_t m_imageArrayLayers = {};
    vk::ImageUsageFlags m_imageUsage = {};
    vk::SurfaceTransformFlagBitsKHR m_preTransform =
        vk::SurfaceTransformFlagBitsKHR::eIdentity;
    vk::CompositeAlphaFlagBitsKHR m_compositeAlpha =
        vk::CompositeAlphaFlagBitsKHR::eOpaque;
    vk::PresentModeKHR m_presentMode = vk::PresentModeKHR::eImmediate;

    std::vector<rad::Ref<VulkanImage>> m_images;
    std::vector<rad::Ref<VulkanImageView>> m_imageViews;
}; // class VulkanSwapchain

} // namespace sd
