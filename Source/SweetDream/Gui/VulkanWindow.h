#pragma once

#include <SweetDream/Gui/Window.h>

#include <SweetDream/Core/API/Vulkan/VulkanCommon.h>
#include <SweetDream/Core/API/Vulkan/VulkanDevice.h>
#include <SweetDream/Core/API/Vulkan/VulkanFence.h>
#include <SweetDream/Core/API/Vulkan/VulkanInstance.h>
#include <SweetDream/Core/API/Vulkan/VulkanSemaphore.h>
#include <SweetDream/Core/API/Vulkan/VulkanSurface.h>
#include <SweetDream/Core/API/Vulkan/VulkanSwapchain.h>

#include <cstdint>
#include <vector>

namespace sd
{

class VulkanImage;
class VulkanImageView;

struct SwapchainConfig
{
    // Preferred VkSwapchainCreateInfoKHR::minImageCount; clamped to surface capabilities.
    uint32_t minImageCount = 3;
    bool vsync = true;
};

enum class FrameStatus
{
    // An image is acquired; getters are valid until Present().
    Ready,
    // The window has no drawable area (for example, while minimized); retry later.
    Skip,
    // Recreate the swapchain and its dependent resources before retrying.
    OutOfDate,
    // Recreate the surface, swapchain, and their dependent resources before retrying.
    SurfaceLost,
    // An unexpected or unrecoverable acquire error occurred.
    Error,
};

class VulkanWindow : public Window
{
public:
    // Maximum outstanding presentation operations (matches vkcube FRAME_LAG).
    static constexpr uint32_t MaxFrameLag = 2;

    VulkanWindow();
    ~VulkanWindow() override;

    VulkanWindow(const VulkanWindow&) = delete;
    VulkanWindow& operator=(const VulkanWindow&) = delete;

    // SDL_WINDOW_VULKAN is always enabled.
    [[nodiscard]] bool Create(const char* title, int width, int height,
                              SDL_WindowFlags flags = 0) override;
    // Releases the swapchain, surface, and device/instance refs before the native window.
    void Destroy() override;

    // Creates a Vulkan presentation surface for this window.
    // Fails while a swapchain image is acquired.
    [[nodiscard]] bool CreateVulkanSurface(rad::Ref<VulkanInstance> instance);
    [[nodiscard]] VulkanSurface* GetVulkanSurface() const noexcept { return m_surface.get(); }
    // True if a graphics queue on the physical device can present to this window.
    [[nodiscard]] bool CanPresent(vk::PhysicalDevice physicalDevice) const;

    // The device must belong to this window's instance and its graphics queue
    // must support presentation to the current surface.
    [[nodiscard]] bool SetDevice(rad::Ref<VulkanDevice> device);
    [[nodiscard]] VulkanDevice* GetDevice() const noexcept { return m_device.get(); }

    // Creates or recreates the swapchain. Surface loss is recovered here.
    [[nodiscard]] bool CreateSwapchain();
    [[nodiscard]] bool CreateSwapchain(const SwapchainConfig& config);
    void DestroySwapchain();
    [[nodiscard]] VulkanSwapchain* GetSwapchain() const noexcept
    {
        return m_swapchain.get();
    }
    [[nodiscard]] const SwapchainConfig& GetSwapchainConfig() const noexcept
    {
        return m_swapchainConfig;
    }
    void SetSwapchainConfig(const SwapchainConfig& config) noexcept
    {
        m_swapchainConfig = config;
    }
    [[nodiscard]] bool IsSwapchainReady() const noexcept
    {
        return (m_swapchain != nullptr) && !m_swapchainOutOfDate;
    }

    // When Ready, swapchain-image getters are valid until Present(). The caller
    // must submit rendering that waits on GetSwapchainImageAcquiredSemaphore(), signals
    // GetRenderCompleteSemaphore(), and uses GetFrameFence() (already reset).
    [[nodiscard]] FrameStatus AcquireNextFrame();
    // Presents the acquired image, waiting on GetRenderCompleteSemaphore().
    // Recoverable results (out of date, surface lost, suboptimal) are recorded
    // for the next AcquireNextFrame. Returns false only on an unrecoverable error.
    [[nodiscard]] bool Present();

    [[nodiscard]] uint32_t GetFrameSlotIndex() const noexcept;
    [[nodiscard]] uint32_t GetSwapchainImageIndex() const noexcept;
    [[nodiscard]] VulkanImage* GetSwapchainImage() const noexcept;
    [[nodiscard]] VulkanImageView* GetSwapchainImageView() const noexcept;
    [[nodiscard]] VulkanSemaphore* GetSwapchainImageAcquiredSemaphore() const noexcept;
    [[nodiscard]] VulkanSemaphore* GetRenderCompleteSemaphore() const noexcept;
    [[nodiscard]] VulkanFence* GetFrameFence() const noexcept;

    void WaitIdle();
    [[nodiscard]] uint32_t GetMaxFrameLag() const noexcept
    {
        return static_cast<uint32_t>(m_frameSlots.size());
    }

private:
    struct FrameSlot
    {
        rad::Ref<VulkanFence> fence;
        rad::Ref<VulkanSemaphore> swapchainImageAcquired;
    };

    [[nodiscard]] bool RecreateLostSurface();
    [[nodiscard]] bool HasDeviceSurfaceSupport(VulkanDevice* device,
                                               VulkanSurface* surface) const;
    [[nodiscard]] bool CreateFrameResources();
    [[nodiscard]] bool CreateRenderCompleteSemaphores();
    void DestroyFrameResources();

    rad::Ref<VulkanInstance> m_instance;
    rad::Ref<VulkanDevice> m_device;
    rad::Ref<VulkanSurface> m_surface;
    rad::Ref<VulkanSwapchain> m_swapchain;
    SwapchainConfig m_swapchainConfig = {};

    // Swapchain-image-acquired semaphores and fences are reused by frame slot;
    // render-complete semaphores are reused by swapchain image.
    std::vector<FrameSlot> m_frameSlots;
    std::vector<rad::Ref<VulkanSemaphore>> m_renderCompleteSemaphores;
    uint32_t m_frameSlotIndex = 0;
    uint32_t m_swapchainImageIndex = 0;
    bool m_swapchainImageAcquired = false;
    // Pixel size last associated with the current swapchain. This may differ from
    // the swapchain image extent when the surface fixes or clamps its extent.
    vk::Extent2D m_sizeInPixels = {};
    // True when the current swapchain must not be acquired. The next
    // AcquireNextFrame reports OutOfDate, or SurfaceLost if m_surfaceLost.
    bool m_swapchainOutOfDate = true;
    bool m_surfaceLost = false;
}; // class VulkanWindow

} // namespace sd
