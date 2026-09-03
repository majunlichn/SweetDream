#include <SweetDream/Core/API/Vulkan/VulkanSwapchain.h>
#include <SweetDream/Core/API/Vulkan/VulkanDevice.h>
#include <SweetDream/Core/API/Vulkan/VulkanFence.h>
#include <SweetDream/Core/API/Vulkan/VulkanImage.h>
#include <SweetDream/Core/API/Vulkan/VulkanSemaphore.h>
#include <SweetDream/Core/API/Vulkan/VulkanSurface.h>

#include <stdexcept>
#include <utility>

namespace sd
{

VulkanSwapchain::VulkanSwapchain(
    rad::Ref<VulkanDevice> device, rad::Ref<VulkanSurface> surface,
    const vk::SwapchainCreateInfoKHR& createInfo) :
    m_device(std::move(device)),
    m_surface(std::move(surface)),
    m_imageFormat(createInfo.imageFormat),
    m_imageColorSpace(createInfo.imageColorSpace),
    m_imageExtent(createInfo.imageExtent),
    m_imageArrayLayers(createInfo.imageArrayLayers),
    m_imageUsage(createInfo.imageUsage),
    m_preTransform(createInfo.preTransform),
    m_compositeAlpha(createInfo.compositeAlpha),
    m_presentMode(createInfo.presentMode)
{
    if (!m_device)
    {
        throw std::invalid_argument("VulkanSwapchain requires a valid VulkanDevice");
    }
    if (!m_surface)
    {
        throw std::invalid_argument("VulkanSwapchain requires a valid VulkanSurface");
    }
    if (m_surface->GetInstance() != m_device->GetInstance())
    {
        throw std::invalid_argument(
            "VulkanSwapchain surface and device must belong to the same VulkanInstance");
    }
    if (createInfo.surface != m_surface->GetHandle())
    {
        throw std::invalid_argument(
            "VulkanSwapchain create info must reference the supplied VulkanSurface");
    }
    if (m_imageArrayLayers == 0)
    {
        throw std::invalid_argument(
            "VulkanSwapchain requires at least one image array layer");
    }

    try
    {
        m_handle =
            m_device->GetHandle().createSwapchainKHR(createInfo, nullptr, GetDispatcher());

        const std::vector<vk::Image> handles =
            m_device->GetHandle().getSwapchainImagesKHR(m_handle, GetDispatcher());
        if (handles.empty())
        {
            throw std::runtime_error("VulkanSwapchain contains no images");
        }

        m_imageCount = static_cast<uint32_t>(handles.size());
        m_images.reserve(handles.size());
        m_imageViews.reserve(handles.size());

        vk::ImageCreateInfo imageCreateInfo;
        imageCreateInfo.imageType = vk::ImageType::e2D;
        imageCreateInfo.format = m_imageFormat;
        imageCreateInfo.extent =
            vk::Extent3D{m_imageExtent.width, m_imageExtent.height, 1};
        imageCreateInfo.mipLevels = 1;
        imageCreateInfo.arrayLayers = m_imageArrayLayers;
        imageCreateInfo.samples = vk::SampleCountFlagBits::e1;
        imageCreateInfo.tiling = vk::ImageTiling::eOptimal;
        imageCreateInfo.usage = m_imageUsage;
        imageCreateInfo.sharingMode = createInfo.imageSharingMode;
        imageCreateInfo.queueFamilyIndexCount = createInfo.queueFamilyIndexCount;
        imageCreateInfo.pQueueFamilyIndices = createInfo.pQueueFamilyIndices;
        imageCreateInfo.initialLayout = vk::ImageLayout::eUndefined;

        const vk::ImageViewType viewType =
            m_imageArrayLayers == 1 ? vk::ImageViewType::e2D
                                    : vk::ImageViewType::e2DArray;
        for (vk::Image handle : handles)
        {
            rad::Ref<VulkanImage> image =
                VulkanImage::WrapBorrowed(m_device, imageCreateInfo, handle);
            rad::Ref<VulkanImageView> imageView =
                image->CreateView(viewType, m_imageFormat);
            m_images.push_back(std::move(image));
            m_imageViews.push_back(std::move(imageView));
        }
    }
    catch (...)
    {
        m_imageViews.clear();
        m_images.clear();
        if (m_handle)
        {
            m_device->GetHandle().destroySwapchainKHR(m_handle, nullptr,
                                                       GetDispatcher());
            m_handle = nullptr;
        }
        throw;
    }
}

VulkanSwapchain::~VulkanSwapchain()
{
    // Waiting here would stall every device queue, so synchronization is the
    // caller's responsibility.
    // Swapchain-backed image views must be destroyed before the swapchain.
    m_imageViews.clear();
    m_images.clear();

    if (m_handle)
    {
        m_device->GetHandle().destroySwapchainKHR(m_handle, nullptr, GetDispatcher());
        m_handle = nullptr;
    }
}

const vk::detail::DispatchLoaderDynamic& VulkanSwapchain::GetDispatcher() const noexcept
{
    return m_device->GetDispatcher();
}

std::pair<vk::Result, uint32_t> VulkanSwapchain::AcquireNextImage(
    uint64_t timeout, VulkanSemaphore* semaphore, VulkanFence* fence,
    uint32_t deviceMask)
{
    if (semaphore == nullptr && fence == nullptr)
    {
        throw std::invalid_argument(
            "AcquireNextImage requires a semaphore or fence");
    }
    if (semaphore != nullptr && semaphore->GetDevice() != m_device.get())
    {
        throw std::invalid_argument(
            "AcquireNextImage semaphore belongs to a different VulkanDevice");
    }
    if (fence != nullptr && fence->GetDevice() != m_device.get())
    {
        throw std::invalid_argument(
            "AcquireNextImage fence belongs to a different VulkanDevice");
    }
    if (deviceMask == 0)
    {
        throw std::invalid_argument("AcquireNextImage device mask must not be zero");
    }

    vk::AcquireNextImageInfoKHR acquireInfo;
    acquireInfo.swapchain = m_handle;
    acquireInfo.timeout = timeout;
    acquireInfo.semaphore =
        semaphore != nullptr ? semaphore->GetHandle() : vk::Semaphore{};
    acquireInfo.fence = fence != nullptr ? fence->GetHandle() : vk::Fence{};
    acquireInfo.deviceMask = deviceMask;

    uint32_t acquiredImageIndex = 0;
    VkResult rawResult = VK_SUCCESS;
    if (GetDispatcher().vkAcquireNextImage2KHR != nullptr)
    {
        rawResult = GetDispatcher().vkAcquireNextImage2KHR(
            static_cast<VkDevice>(m_device->GetHandle()),
            reinterpret_cast<const VkAcquireNextImageInfoKHR*>(&acquireInfo),
            &acquiredImageIndex);
    }
    else
    {
        if (deviceMask != 1)
        {
            throw std::runtime_error(
                "AcquireNextImage requires Vulkan 1.1 or VK_KHR_device_group "
                "for a non-default device mask");
        }
        rawResult = GetDispatcher().vkAcquireNextImageKHR(
            static_cast<VkDevice>(m_device->GetHandle()),
            static_cast<VkSwapchainKHR>(m_handle), timeout,
            static_cast<VkSemaphore>(acquireInfo.semaphore),
            static_cast<VkFence>(acquireInfo.fence), &acquiredImageIndex);
    }
    const vk::Result result = static_cast<vk::Result>(rawResult);
    if (result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR)
    {
        return {result, acquiredImageIndex};
    }
    return {result, UINT32_MAX};
}

} // namespace sd
