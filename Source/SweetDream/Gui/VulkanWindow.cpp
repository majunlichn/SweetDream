#include <SweetDream/Gui/VulkanWindow.h>

#include <SweetDream/Core/API/Vulkan/VulkanDevice.h>
#include <SweetDream/Core/API/Vulkan/VulkanFence.h>
#include <SweetDream/Core/API/Vulkan/VulkanImage.h>
#include <SweetDream/Core/API/Vulkan/VulkanInstance.h>
#include <SweetDream/Core/API/Vulkan/VulkanQueue.h>
#include <SweetDream/Core/API/Vulkan/VulkanSemaphore.h>
#include <SweetDream/Core/API/Vulkan/VulkanSurface.h>
#include <SweetDream/Core/API/Vulkan/VulkanSwapchain.h>
#include <SweetDream/Core/IO/Logging.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <algorithm>
#include <limits>
#include <span>
#include <stdexcept>
#include <utility>

namespace sd
{
namespace
{

vk::SurfaceFormatKHR PickSurfaceFormat(std::span<const vk::SurfaceFormatKHR> surfaceFormats)
{
    // Prefer non-sRGB formats (matches vkcube pick_surface_format).
    constexpr vk::Format preferredFormats[] = {
        vk::Format::eR8G8B8A8Unorm,
        vk::Format::eB8G8R8A8Unorm,
        vk::Format::eA2B10G10R10UnormPack32,
        vk::Format::eA2R10G10B10UnormPack32,
        vk::Format::eA1R5G5B5UnormPack16,
        vk::Format::eR5G6B5UnormPack16,
        vk::Format::eR16G16B16A16Sfloat,
    };

    for (vk::Format preferred : preferredFormats)
    {
        for (const vk::SurfaceFormatKHR& surfaceFormat : surfaceFormats)
        {
            if (surfaceFormat.format == preferred)
            {
                return surfaceFormat;
            }
        }
    }

    SD_LOG(warn,
           "Preferred swapchain formats unavailable; falling back to the first exposed format");
    return surfaceFormats.front();
}

bool Contains(std::span<const vk::PresentModeKHR> presentModes, vk::PresentModeKHR mode)
{
    return std::find(presentModes.begin(), presentModes.end(), mode) != presentModes.end();
}

vk::PresentModeKHR SelectPresentMode(std::span<const vk::PresentModeKHR> presentModes,
                                     uint32_t imageCount, bool vsync)
{
    if (vsync)
    {
        // MAILBOX needs a third image to replace the pending present with lower latency than FIFO.
        if ((imageCount >= 3) && Contains(presentModes, vk::PresentModeKHR::eMailbox))
        {
            return vk::PresentModeKHR::eMailbox;
        }
        // FIFO is guaranteed by the Vulkan spec.
        return vk::PresentModeKHR::eFifo;
    }

    if (Contains(presentModes, vk::PresentModeKHR::eImmediate))
    {
        return vk::PresentModeKHR::eImmediate;
    }
    if (Contains(presentModes, vk::PresentModeKHR::eMailbox))
    {
        return vk::PresentModeKHR::eMailbox;
    }
    if (Contains(presentModes, vk::PresentModeKHR::eFifoRelaxed))
    {
        return vk::PresentModeKHR::eFifoRelaxed;
    }
    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D SelectSwapchainExtent(const vk::SurfaceCapabilitiesKHR& capabilities,
                                   vk::Extent2D windowSizeInPixels)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return capabilities.currentExtent;
    }

    return vk::Extent2D{
        std::clamp(windowSizeInPixels.width, capabilities.minImageExtent.width,
                   capabilities.maxImageExtent.width),
        std::clamp(windowSizeInPixels.height, capabilities.minImageExtent.height,
                   capabilities.maxImageExtent.height)};
}

vk::CompositeAlphaFlagBitsKHR SelectCompositeAlpha(
    vk::CompositeAlphaFlagsKHR supportedCompositeAlpha)
{
    constexpr vk::CompositeAlphaFlagBitsKHR candidates[] = {
        vk::CompositeAlphaFlagBitsKHR::eOpaque,
        vk::CompositeAlphaFlagBitsKHR::ePreMultiplied,
        vk::CompositeAlphaFlagBitsKHR::ePostMultiplied,
        vk::CompositeAlphaFlagBitsKHR::eInherit,
    };

    for (vk::CompositeAlphaFlagBitsKHR candidate : candidates)
    {
        if (HasAllBits(supportedCompositeAlpha, candidate))
        {
            return candidate;
        }
    }

    return vk::CompositeAlphaFlagBitsKHR::eOpaque;
}

bool IsSurfaceLost(const vk::SystemError& exception)
{
    return exception.code() == vk::make_error_code(vk::Result::eErrorSurfaceLostKHR);
}

} // namespace

VulkanWindow::VulkanWindow() = default;

VulkanWindow::~VulkanWindow()
{
    Destroy();
}

bool VulkanWindow::Create(const char* title, int width, int height, SDL_WindowFlags flags)
{
    return Window::Create(title, width, height, flags | SDL_WINDOW_VULKAN);
}

void VulkanWindow::Destroy()
{
    DestroySwapchain();
    m_surface.reset();
    m_device.reset();
    m_instance.reset();
    m_surfaceLost = false;
    Window::Destroy();
}

bool VulkanWindow::CreateVulkanSurface(rad::Ref<VulkanInstance> instance)
{
    if (m_swapchainImageAcquired)
    {
        SD_LOG(err, "VulkanWindow::CreateVulkanSurface called while a swapchain image is acquired");
        return false;
    }
    if (!IsCreated())
    {
        SD_LOG(err, "VulkanWindow::CreateVulkanSurface requires a created window");
        return false;
    }
    if ((GetFlags() & SDL_WINDOW_VULKAN) == 0)
    {
        SD_LOG(err, "VulkanWindow::CreateVulkanSurface requires SDL_WINDOW_VULKAN");
        return false;
    }
    if (!instance)
    {
        SD_LOG(err, "VulkanWindow::CreateVulkanSurface requires a valid VulkanInstance");
        return false;
    }
    if (m_instance && (m_instance.get() != instance.get()))
    {
        SD_LOG(err, "VulkanWindow::CreateVulkanSurface cannot change VulkanInstance");
        return false;
    }
    if (m_surface)
    {
        return true;
    }

    const bool hadInstance = m_instance != nullptr;
    m_instance = std::move(instance);

    VkSurfaceKHR handle = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(GetHandle(), static_cast<VkInstance>(m_instance->GetHandle()),
                                 nullptr, &handle))
    {
        SD_LOG(err, "SDL_Vulkan_CreateSurface failed: {}", SDL_GetError());
        if (!hadInstance)
        {
            m_instance.reset();
        }
        return false;
    }

    try
    {
        m_surface = VulkanSurface::Create(m_instance, vk::SurfaceKHR{handle});
    }
    catch (const std::exception& exception)
    {
        m_instance->GetDispatcher().vkDestroySurfaceKHR(
            static_cast<VkInstance>(m_instance->GetHandle()), handle, nullptr);
        SD_LOG(err, "VulkanWindow::CreateVulkanSurface failed: {}", exception.what());
        if (!hadInstance)
        {
            m_instance.reset();
        }
        return false;
    }

    m_surfaceLost = false;
    return true;
}

bool VulkanWindow::CanPresent(vk::PhysicalDevice physicalDevice) const
{
    if (!m_instance || !m_surface || !physicalDevice)
    {
        return false;
    }

    try
    {
        const vk::SurfaceKHR surface = m_surface->GetHandle();
        const auto& dispatcher = m_instance->GetDispatcher();
        const std::vector<vk::QueueFamilyProperties> queueFamilies =
            physicalDevice.getQueueFamilyProperties(dispatcher);
        for (uint32_t queueFamilyIndex = 0; queueFamilyIndex < queueFamilies.size();
             ++queueFamilyIndex)
        {
            const vk::QueueFamilyProperties& properties = queueFamilies[queueFamilyIndex];
            if ((properties.queueCount > 0) &&
                HasAllBits(properties.queueFlags, vk::QueueFlagBits::eGraphics) &&
                physicalDevice.getSurfaceSupportKHR(queueFamilyIndex, surface, dispatcher))
            {
                return true;
            }
        }
    }
    catch (const std::exception& exception)
    {
        SD_LOG(err, "VulkanWindow::CanPresent failed: {}", exception.what());
    }
    return false;
}

bool VulkanWindow::SetDevice(rad::Ref<VulkanDevice> device)
{
    if (!device)
    {
        SD_LOG(err, "VulkanWindow::SetDevice requires a valid VulkanDevice");
        return false;
    }
    if (!m_instance)
    {
        SD_LOG(err, "VulkanWindow::SetDevice requires a Vulkan instance");
        return false;
    }
    if (device->GetInstance() != m_instance.get())
    {
        SD_LOG(err,
               "VulkanWindow::SetDevice requires a device created from this window's instance");
        return false;
    }
    if (!m_surface)
    {
        SD_LOG(err, "VulkanWindow::SetDevice requires a Vulkan surface");
        return false;
    }
    if (!HasDeviceSurfaceSupport(device.get(), m_surface.get()))
    {
        SD_LOG(err,
               "VulkanWindow::SetDevice requires a graphics queue that can present to the "
               "window surface");
        return false;
    }
    if (m_swapchainImageAcquired)
    {
        SD_LOG(err, "VulkanWindow::SetDevice called while a swapchain image is acquired");
        return false;
    }
    if (m_device.get() == device.get())
    {
        return true;
    }

    DestroySwapchain();
    m_device = std::move(device);
    return true;
}

bool VulkanWindow::HasDeviceSurfaceSupport(VulkanDevice* device, VulkanSurface* surface) const
{
    if ((device == nullptr) || (surface == nullptr) ||
        !device->HasQueueFamily(VulkanQueueFamily::Graphics))
    {
        return false;
    }

    try
    {
        return device->GetSurfaceSupport(surface->GetHandle(),
                                         device->GetQueueFamilyIndex(VulkanQueueFamily::Graphics));
    }
    catch (const std::exception& exception)
    {
        SD_LOG(err, "VulkanWindow surface support query failed: {}", exception.what());
        return false;
    }
}

bool VulkanWindow::RecreateLostSurface()
{
    if (!m_surfaceLost)
    {
        return m_surface != nullptr;
    }
    if (!IsCreated() || ((GetFlags() & SDL_WINDOW_VULKAN) == 0) || !m_instance)
    {
        SD_LOG(err,
               "VulkanWindow cannot recover a lost surface without a created Vulkan window "
               "and instance");
        return false;
    }

    DestroySwapchain();
    m_surface.reset();

    if (!CreateVulkanSurface(m_instance))
    {
        return false;
    }
    if (m_device && !HasDeviceSurfaceSupport(m_device.get(), m_surface.get()))
    {
        SD_LOG(err,
               "VulkanWindow device graphics queue cannot present to the recreated surface");
        m_surface.reset();
        m_surfaceLost = true;
        return false;
    }

    return true;
}

bool VulkanWindow::CreateSwapchain(const SwapchainConfig& config)
{
    m_swapchainConfig = config;
    return CreateSwapchain();
}

bool VulkanWindow::CreateSwapchain()
{
    if (m_swapchainImageAcquired)
    {
        SD_LOG(err, "VulkanWindow::CreateSwapchain called while a swapchain image is acquired");
        return false;
    }
    if (m_surfaceLost && !RecreateLostSurface())
    {
        return false;
    }
    if (!IsCreated() || !m_device || !m_surface)
    {
        SD_LOG(err,
               "VulkanWindow::CreateSwapchain requires a created window, Vulkan surface, "
               "and device");
        return false;
    }
    if ((GetFlags() & SDL_WINDOW_VULKAN) == 0)
    {
        SD_LOG(err, "VulkanWindow::CreateSwapchain requires SDL_WINDOW_VULKAN");
        return false;
    }
    if (!HasDeviceSurfaceSupport(m_device.get(), m_surface.get()))
    {
        SD_LOG(err,
               "VulkanWindow::CreateSwapchain requires a graphics queue that can present "
               "to the window surface");
        return false;
    }

    const SwapchainConfig& config = m_swapchainConfig;
    if (config.minImageCount == 0)
    {
        SD_LOG(err, "VulkanWindow::CreateSwapchain requires minImageCount > 0");
        return false;
    }

    int pixelWidth = 0;
    int pixelHeight = 0;
    if (!GetSizeInPixels(&pixelWidth, &pixelHeight) || (pixelWidth <= 0) || (pixelHeight <= 0))
    {
        SD_LOG(warn,
               "VulkanWindow::CreateSwapchain skipped because the window pixel extent is "
               "zero");
        return false;
    }

    vk::SurfaceCapabilitiesKHR capabilities;
    std::vector<vk::SurfaceFormatKHR> surfaceFormats;
    std::vector<vk::PresentModeKHR> presentModes;
    try
    {
        const vk::SurfaceKHR surface = m_surface->GetHandle();
        capabilities = m_device->GetCapabilities(surface);
        surfaceFormats = m_device->GetSurfaceFormats(surface);
        presentModes = m_device->GetPresentModes(surface);
    }
    catch (const vk::SystemError& exception)
    {
        if (IsSurfaceLost(exception))
        {
            m_surfaceLost = true;
            m_swapchainOutOfDate = true;
            SD_LOG(err, "VulkanWindow::CreateSwapchain surface lost");
        }
        else
        {
            SD_LOG(err, "VulkanWindow::CreateSwapchain surface query failed: {}",
                   exception.what());
        }
        return false;
    }

    if ((capabilities.maxImageExtent.width == 0) || (capabilities.maxImageExtent.height == 0))
    {
        SD_LOG(warn,
               "VulkanWindow::CreateSwapchain skipped because the surface extent is zero");
        return false;
    }
    if (surfaceFormats.empty())
    {
        SD_LOG(err, "VulkanWindow::CreateSwapchain found no surface formats");
        return false;
    }
    if (presentModes.empty())
    {
        SD_LOG(err, "VulkanWindow::CreateSwapchain found no present modes");
        return false;
    }

    const vk::Extent2D windowSizeInPixels{static_cast<uint32_t>(pixelWidth),
                                          static_cast<uint32_t>(pixelHeight)};
    const vk::Extent2D extent = SelectSwapchainExtent(capabilities, windowSizeInPixels);

    uint32_t minImageCount = std::max(config.minImageCount, capabilities.minImageCount);
    if (capabilities.maxImageCount > 0)
    {
        minImageCount = std::min(minImageCount, capabilities.maxImageCount);
    }
    if (minImageCount < config.minImageCount)
    {
        SD_LOG(warn, "Swapchain minImageCount clamped from {} to {} (surface min={}, max={})",
               config.minImageCount, minImageCount, capabilities.minImageCount,
               capabilities.maxImageCount);
    }

    const vk::SurfaceFormatKHR surfaceFormat = PickSurfaceFormat(surfaceFormats);
    const vk::PresentModeKHR presentMode =
        SelectPresentMode(presentModes, minImageCount, config.vsync);
    const vk::SurfaceTransformFlagBitsKHR preTransform =
        HasAllBits(capabilities.supportedTransforms, vk::SurfaceTransformFlagBitsKHR::eIdentity)
            ? vk::SurfaceTransformFlagBitsKHR::eIdentity
            : capabilities.currentTransform;
    const vk::CompositeAlphaFlagBitsKHR compositeAlpha =
        SelectCompositeAlpha(capabilities.supportedCompositeAlpha);

    if (m_swapchain || !m_frameSlots.empty() || !m_renderCompleteSemaphores.empty())
    {
        try
        {
            WaitIdle();
        }
        catch (const std::exception& exception)
        {
            SD_LOG(err, "VulkanWindow::CreateSwapchain WaitIdle failed: {}", exception.what());
            return false;
        }
    }

    vk::SwapchainCreateInfoKHR createInfo;
    createInfo.surface = m_surface->GetHandle();
    createInfo.minImageCount = minImageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
    createInfo.imageSharingMode = vk::SharingMode::eExclusive;
    createInfo.preTransform = preTransform;
    createInfo.compositeAlpha = compositeAlpha;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = m_swapchain ? m_swapchain->GetHandle() : vk::SwapchainKHR{};

    // Vulkan retires a non-null oldSwapchain as soon as vkCreateSwapchainKHR is
    // called, even if creation fails, so a failed replacement must not restore it.
    rad::Ref<VulkanSwapchain> previous = std::move(m_swapchain);
    try
    {
        m_swapchain = m_device->CreateSwapchain(m_surface, createInfo);
    }
    catch (const vk::SystemError& exception)
    {
        DestroySwapchain();
        if (IsSurfaceLost(exception))
        {
            m_surfaceLost = true;
            SD_LOG(err, "VulkanWindow::CreateSwapchain surface lost");
        }
        else
        {
            SD_LOG(err, "VulkanWindow::CreateSwapchain failed: {}", exception.what());
        }
        return false;
    }
    catch (const std::exception& exception)
    {
        DestroySwapchain();
        SD_LOG(err, "VulkanWindow::CreateSwapchain failed: {}", exception.what());
        return false;
    }

    // The replacement is valid. Rebuild synchronization before releasing the
    // retired swapchain and its borrowed images/views.
    m_swapchainOutOfDate = true;
    m_swapchainImageAcquired = false;
    m_renderCompleteSemaphores.clear();
    DestroyFrameResources();

    if (!CreateFrameResources() || !CreateRenderCompleteSemaphores())
    {
        m_renderCompleteSemaphores.clear();
        DestroyFrameResources();
        m_swapchain.reset();
        return false;
    }

    previous.reset();
    m_frameSlotIndex = 0;
    m_surfaceLost = false;
    m_sizeInPixels = windowSizeInPixels;
    m_swapchainOutOfDate = false;
    SD_LOG(info, "Swapchain created: {}x{}, {} images, format={}, presentMode={}, vsync={}",
           m_swapchain->GetImageWidth(), m_swapchain->GetImageHeight(),
           m_swapchain->GetImageCount(), vk::to_string(m_swapchain->GetImageFormat()),
           vk::to_string(m_swapchain->GetPresentMode()), config.vsync);
    return true;
}

void VulkanWindow::WaitIdle()
{
    if (m_device)
    {
        m_device->WaitIdle();
    }
}

void VulkanWindow::DestroySwapchain()
{
    // Sync objects and the swapchain may still be in flight.
    if (m_device &&
        (m_swapchain || !m_frameSlots.empty() || !m_renderCompleteSemaphores.empty()))
    {
        try
        {
            WaitIdle();
        }
        catch (const std::exception& exception)
        {
            SD_LOG(err, "VulkanWindow::DestroySwapchain WaitIdle failed: {}", exception.what());
        }
    }

    m_swapchainImageAcquired = false;
    m_sizeInPixels = vk::Extent2D{};
    m_swapchainOutOfDate = true;
    m_renderCompleteSemaphores.clear();
    DestroyFrameResources();
    m_swapchain.reset();
}

bool VulkanWindow::CreateFrameResources()
{
    if (!m_device)
    {
        return false;
    }

    try
    {
        std::vector<FrameSlot> frameSlots;
        frameSlots.reserve(MaxFrameLag);
        for (uint32_t i = 0; i < MaxFrameLag; ++i)
        {
            FrameSlot slot;
            slot.fence = m_device->CreateFence(true);
            slot.swapchainImageAcquired = m_device->CreateSemaphore();
            frameSlots.push_back(std::move(slot));
        }

        m_frameSlots = std::move(frameSlots);
    }
    catch (const std::exception& exception)
    {
        SD_LOG(err, "VulkanWindow::CreateFrameResources failed: {}", exception.what());
        DestroyFrameResources();
        return false;
    }

    return true;
}

bool VulkanWindow::CreateRenderCompleteSemaphores()
{
    if (!m_device || !m_swapchain)
    {
        return false;
    }

    try
    {
        std::vector<rad::Ref<VulkanSemaphore>> semaphores;
        semaphores.reserve(m_swapchain->GetImageCount());
        for (uint32_t i = 0; i < m_swapchain->GetImageCount(); ++i)
        {
            semaphores.push_back(m_device->CreateSemaphore());
        }
        m_renderCompleteSemaphores = std::move(semaphores);
    }
    catch (const std::exception& exception)
    {
        SD_LOG(err, "VulkanWindow::CreateRenderCompleteSemaphores failed: {}",
               exception.what());
        m_renderCompleteSemaphores.clear();
        return false;
    }
    return true;
}

void VulkanWindow::DestroyFrameResources()
{
    m_frameSlots.clear();
    m_frameSlotIndex = 0;
}

uint32_t VulkanWindow::GetFrameSlotIndex() const noexcept
{
    return m_frameSlotIndex;
}

uint32_t VulkanWindow::GetSwapchainImageIndex() const noexcept
{
    return m_swapchainImageIndex;
}

VulkanImage* VulkanWindow::GetSwapchainImage() const noexcept
{
    if (!m_swapchainImageAcquired || !m_swapchain ||
        (m_swapchainImageIndex >= m_swapchain->GetImageCount()))
    {
        return nullptr;
    }
    return m_swapchain->GetImage(m_swapchainImageIndex);
}

VulkanImageView* VulkanWindow::GetSwapchainImageView() const noexcept
{
    if (!m_swapchainImageAcquired || !m_swapchain ||
        (m_swapchainImageIndex >= m_swapchain->GetImageCount()))
    {
        return nullptr;
    }
    return m_swapchain->GetImageView(m_swapchainImageIndex);
}

VulkanSemaphore* VulkanWindow::GetSwapchainImageAcquiredSemaphore() const noexcept
{
    if (!m_swapchainImageAcquired || (m_frameSlotIndex >= m_frameSlots.size()))
    {
        return nullptr;
    }
    return m_frameSlots[m_frameSlotIndex].swapchainImageAcquired.get();
}

VulkanSemaphore* VulkanWindow::GetRenderCompleteSemaphore() const noexcept
{
    if (!m_swapchainImageAcquired ||
        (m_swapchainImageIndex >= m_renderCompleteSemaphores.size()))
    {
        return nullptr;
    }
    return m_renderCompleteSemaphores[m_swapchainImageIndex].get();
}

VulkanFence* VulkanWindow::GetFrameFence() const noexcept
{
    if (!m_swapchainImageAcquired || (m_frameSlotIndex >= m_frameSlots.size()))
    {
        return nullptr;
    }
    return m_frameSlots[m_frameSlotIndex].fence.get();
}

FrameStatus VulkanWindow::AcquireNextFrame()
{
    if (m_swapchainImageAcquired)
    {
        SD_LOG(err, "VulkanWindow::AcquireNextFrame called while a swapchain image is already acquired");
        return FrameStatus::Error;
    }
    if (!IsCreated() || !m_device || !m_surface)
    {
        SD_LOG(err, "VulkanWindow::AcquireNextFrame called before initialization");
        return FrameStatus::Error;
    }

    const SDL_WindowFlags windowFlags = GetFlags();
    if ((windowFlags & SDL_WINDOW_VULKAN) == 0)
    {
        SD_LOG(err, "VulkanWindow::AcquireNextFrame requires SDL_WINDOW_VULKAN");
        return FrameStatus::Error;
    }
    if ((windowFlags & SDL_WINDOW_MINIMIZED) != 0)
    {
        return FrameStatus::Skip;
    }
    if (!IsSwapchainReady() || m_frameSlots.empty())
    {
        return m_surfaceLost ? FrameStatus::SurfaceLost
                             : FrameStatus::OutOfDate;
    }

    int pixelWidth = 0;
    int pixelHeight = 0;
    if (!GetSizeInPixels(&pixelWidth, &pixelHeight))
    {
        SD_LOG(err, "VulkanWindow::AcquireNextFrame failed to query the window pixel size");
        return FrameStatus::Error;
    }
    if ((pixelWidth <= 0) || (pixelHeight <= 0))
    {
        // Keep the current swapchain, matching vkcube's is_minimized path.
        return FrameStatus::Skip;
    }
    // Compare SDL pixel sizes rather than the swapchain extent: the surface may
    // legitimately fix or clamp the extent to a different value.
    if ((m_sizeInPixels.width != static_cast<uint32_t>(pixelWidth)) ||
        (m_sizeInPixels.height != static_cast<uint32_t>(pixelHeight)))
    {
        m_swapchainOutOfDate = true;
        return FrameStatus::OutOfDate;
    }

    if (m_frameSlotIndex >= m_frameSlots.size())
    {
        m_frameSlotIndex = 0;
    }
    FrameSlot& frame = m_frameSlots[m_frameSlotIndex];

    try
    {
        const vk::Result waitResult = frame.fence->Wait();
        if (waitResult != vk::Result::eSuccess)
        {
            SD_LOG(err, "VulkanWindow::AcquireNextFrame fence wait failed: {}",
                   vk::to_string(waitResult));
            return FrameStatus::Error;
        }
    }
    catch (const std::exception& exception)
    {
        SD_LOG(err, "VulkanWindow::AcquireNextFrame fence wait failed: {}", exception.what());
        return FrameStatus::Error;
    }

    const auto [acquireResult, acquiredImageIndex] =
        m_swapchain->AcquireNextImage(UINT64_MAX, frame.swapchainImageAcquired.get(), nullptr);
    if (acquireResult == vk::Result::eErrorOutOfDateKHR)
    {
        m_swapchainOutOfDate = true;
        return FrameStatus::OutOfDate;
    }
    if (acquireResult == vk::Result::eErrorSurfaceLostKHR)
    {
        m_swapchainOutOfDate = true;
        m_surfaceLost = true;
        SD_LOG(err, "VulkanWindow::AcquireNextFrame surface lost");
        return FrameStatus::SurfaceLost;
    }
    if ((acquireResult != vk::Result::eSuccess) && (acquireResult != vk::Result::eSuboptimalKHR))
    {
        SD_LOG(err, "VulkanWindow::AcquireNextFrame acquire failed: {}",
               vk::to_string(acquireResult));
        return FrameStatus::Error;
    }
    if ((acquiredImageIndex >= m_swapchain->GetImageCount()) ||
        (acquiredImageIndex >= m_renderCompleteSemaphores.size()))
    {
        m_swapchainOutOfDate = true;
        SD_LOG(err, "VulkanWindow::AcquireNextFrame acquired an invalid image index");
        return FrameStatus::Error;
    }

    m_swapchainImageIndex = acquiredImageIndex;
    try
    {
        frame.fence->Reset();
    }
    catch (const std::exception& exception)
    {
        m_swapchainImageAcquired = false;
        m_swapchainOutOfDate = true;
        SD_LOG(err, "VulkanWindow::AcquireNextFrame fence reset failed: {}", exception.what());
        try
        {
            WaitIdle();
        }
        catch (const std::exception& waitException)
        {
            SD_LOG(err, "VulkanWindow::AcquireNextFrame WaitIdle failed: {}",
                   waitException.what());
        }
        return FrameStatus::Error;
    }
    m_swapchainImageAcquired = true;
    return FrameStatus::Ready;
}

bool VulkanWindow::Present()
{
    if (!m_swapchainImageAcquired)
    {
        throw std::logic_error("VulkanWindow::Present called without AcquireNextFrame");
    }
    if (!m_device || !m_swapchain || !m_device->HasQueueFamily(VulkanQueueFamily::Graphics) ||
        (m_frameSlotIndex >= m_frameSlots.size()) ||
        (m_swapchainImageIndex >= m_swapchain->GetImageCount()) ||
        (m_swapchainImageIndex >= m_renderCompleteSemaphores.size()))
    {
        m_swapchainImageAcquired = false;
        m_swapchainOutOfDate = true;
        return false;
    }

    VulkanSemaphore* renderComplete =
        m_renderCompleteSemaphores[m_swapchainImageIndex].get();

    // Recoverable presentation results are recorded for the next AcquireNextFrame.
    const vk::Result presentResult =
        m_device->GetQueue(VulkanQueueFamily::Graphics)
            ->Present(m_swapchain.get(), m_swapchainImageIndex, renderComplete);

    m_swapchainImageAcquired = false;
    m_frameSlotIndex = (m_frameSlotIndex + 1) % static_cast<uint32_t>(m_frameSlots.size());

    if (presentResult == vk::Result::eErrorOutOfDateKHR)
    {
        m_swapchainOutOfDate = true;
        return true;
    }
    if (presentResult == vk::Result::eSuboptimalKHR)
    {
        // SUBOPTIMAL is still presentable. Recreate only when current surface
        // capabilities select a different extent, avoiding persistent rebuild loops.
        if (!IsCreated() || !m_surface)
        {
            m_swapchainOutOfDate = true;
            return true;
        }

        int pixelWidth = 0;
        int pixelHeight = 0;
        if (!GetSizeInPixels(&pixelWidth, &pixelHeight) || (pixelWidth <= 0) || (pixelHeight <= 0))
        {
            m_swapchainOutOfDate = true;
            return true;
        }

        try
        {
            const vk::SurfaceCapabilitiesKHR capabilities =
                m_device->GetCapabilities(m_surface->GetHandle());
            if ((capabilities.maxImageExtent.width == 0) ||
                (capabilities.maxImageExtent.height == 0))
            {
                m_swapchainOutOfDate = true;
                return true;
            }

            const vk::Extent2D windowSizeInPixels{static_cast<uint32_t>(pixelWidth),
                                                  static_cast<uint32_t>(pixelHeight)};
            const vk::Extent2D expectedExtent =
                SelectSwapchainExtent(capabilities, windowSizeInPixels);
            const vk::Extent2D swapchainExtent = m_swapchain->GetImageExtent();
            if ((swapchainExtent.width != expectedExtent.width) ||
                (swapchainExtent.height != expectedExtent.height))
            {
                m_swapchainOutOfDate = true;
            }
            else
            {
                m_sizeInPixels = windowSizeInPixels;
            }
            return true;
        }
        catch (const vk::SystemError& exception)
        {
            m_swapchainOutOfDate = true;
            if (IsSurfaceLost(exception))
            {
                m_surfaceLost = true;
                SD_LOG(err, "VulkanWindow::Present surface lost");
            }
            else
            {
                SD_LOG(err, "VulkanWindow::Present surface query failed: {}", exception.what());
            }
            return true;
        }
    }
    if (presentResult == vk::Result::eErrorSurfaceLostKHR)
    {
        m_swapchainOutOfDate = true;
        m_surfaceLost = true;
        SD_LOG(err, "VulkanWindow::Present surface lost");
        return true;
    }
    if (rad::UnderlyingCast(presentResult) < 0)
    {
        m_swapchainOutOfDate = true;
        SD_LOG(err, "VulkanWindow::Present failed: {}", vk::to_string(presentResult));
        return false;
    }
    return true;
}

} // namespace sd
