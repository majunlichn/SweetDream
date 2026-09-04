#pragma once

#include <SweetDream/Core/API/Vulkan/VulkanCommon.h>

#include <imgui.h>

#include <SDL3/SDL_events.h>

struct ImPlotContext;

namespace sd
{

class VulkanCommandBuffer;
class VulkanDescriptorPool;
class VulkanWindow;

// Dear ImGui context bound to a VulkanWindow (SDL3 platform + Vulkan renderer).
// Record with Render() inside a non-MSAA dynamic-rendering pass whose color
// attachment format is provided by the window swapchain / OnRenderTargetChanged().
class GuiContext
{
public:
    GuiContext();
    ~GuiContext();

    GuiContext(const GuiContext&) = delete;
    GuiContext& operator=(const GuiContext&) = delete;
    GuiContext(GuiContext&&) = delete;
    GuiContext& operator=(GuiContext&&) = delete;

    // Window must already have a device and swapchain (queue, frame lag, color format).
    // descriptorPool is optional: if null, ImGui creates a minimal internal pool.
    // A user pool must have VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, and
    // at least IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE SAMPLED_IMAGE
    // descriptors plus IMGUI_IMPL_VULKAN_MINIMUM_SAMPLER_POOL_SIZE SAMPLER
    // descriptors (see imgui_impl_vulkan.h; currently 8 and 2).
    [[nodiscard]] bool Init(VulkanWindow* window);
    [[nodiscard]] bool Init(VulkanWindow* window, rad::Ref<VulkanDescriptorPool> descriptorPool);
    void Shutdown();

    [[nodiscard]] VulkanWindow* GetWindow() const noexcept { return m_window; }

    [[nodiscard]] ImGuiIO& GetIO() const;
    [[nodiscard]] ImGuiStyle& GetStyle() const;

    // Optional; ImGui embeds a default font if none are added.
    ImFont* AddFontFromFileTTF(const char* filename, float sizePixels = 0.0f,
                               const ImFontConfig* fontCfg = nullptr,
                               const ImWchar* glyphRanges = nullptr);

    // Feed SDL events before NewFrame(). Check GetIO().WantCapture* to gate app input.
    bool ProcessEvent(const SDL_Event& event);

    void NewFrame();
    // Requires an active non-MSAA color attachment matching the Init/OnRenderTargetChanged format.
    void Render(VulkanCommandBuffer* commandBuffer);

    // Rebuild the ImGui pipeline when the UI color attachment format changes.
    // No-op if unchanged. Device should be idle (e.g. after recreating the target).
    void OnRenderTargetChanged(vk::Format colorFormat);

    // Call after VulkanWindow has recreated the swapchain (device idle).
    // ImGui's in-flight buffer count follows MaxFrameLag, not swapchain image
    // count, so this is a no-op unless that lag changes. Format: OnRenderTargetChanged().
    void OnSwapchainRecreated();

private:
    void MakeCurrent() const;
    void CreateMainPipeline();

    // Non-owning; the window must outlive this initialized context.
    VulkanWindow* m_window = nullptr;
    ImGuiContext* m_imgui = nullptr;
    ImPlotContext* m_implot = nullptr;
    rad::Ref<VulkanDescriptorPool> m_descriptorPool;
    vk::Format m_colorFormat = vk::Format::eUndefined;
}; // class GuiContext

} // namespace sd
