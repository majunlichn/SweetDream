#include <SweetDream/Gui/GuiContext.h>
#include <SweetDream/Gui/VulkanWindow.h>

#include <SweetDream/Core/API/Vulkan/VulkanCommand.h>
#include <SweetDream/Core/API/Vulkan/VulkanDescriptor.h>
#include <SweetDream/Core/API/Vulkan/VulkanDevice.h>
#include <SweetDream/Core/API/Vulkan/VulkanInstance.h>
#include <SweetDream/Core/API/Vulkan/VulkanQueue.h>
#include <SweetDream/Core/IO/Logging.h>

#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>
#include <implot.h>

#include <algorithm>
#include <cassert>
#include <exception>
#include <utility>

namespace sd
{
namespace
{

void CheckVkResult(VkResult err)
{
    if (err < 0)
    {
        SD_LOG(err, "ImGui Vulkan: {}", vk::to_string(static_cast<vk::Result>(err)));
    }
}

} // namespace

GuiContext::GuiContext() = default;

GuiContext::~GuiContext()
{
    Shutdown();
}

bool GuiContext::Init(VulkanWindow* window)
{
    return Init(window, {});
}

bool GuiContext::Init(VulkanWindow* window, rad::Ref<VulkanDescriptorPool> descriptorPool)
{
    if (m_imgui != nullptr)
    {
        SD_LOG(err, "GuiContext::Init called while already initialized");
        return false;
    }
    if ((window == nullptr) || !window->IsCreated())
    {
        SD_LOG(err, "GuiContext::Init requires a created window");
        return false;
    }
    if ((window->GetDevice() == nullptr) || (window->GetSwapchain() == nullptr))
    {
        SD_LOG(err, "GuiContext::Init requires a window with a device and swapchain");
        return false;
    }
    const vk::Format colorFormat = window->GetSwapchain()->GetImageFormat();
    if (colorFormat == vk::Format::eUndefined)
    {
        SD_LOG(err, "GuiContext::Init requires a valid swapchain color format");
        return false;
    }

    VulkanDevice* device = window->GetDevice();
    VulkanInstance* instance = device->GetInstance();
    if (instance == nullptr)
    {
        SD_LOG(err, "GuiContext requires a Vulkan instance");
        return false;
    }
    if (!device->HasQueueFamily(VulkanQueueFamily::Graphics))
    {
        SD_LOG(err, "GuiContext requires a graphics queue");
        return false;
    }
    const uint32_t frameLag = window->GetMaxFrameLag();
    if (frameLag < 2)
    {
        SD_LOG(err, "GuiContext requires window MaxFrameLag >= 2 (got {})", frameLag);
        return false;
    }
    if (descriptorPool)
    {
        if (descriptorPool->GetDevice() != device)
        {
            SD_LOG(err, "GuiContext descriptor pool must belong to the window device");
            return false;
        }
        if (!HasAllBits(descriptorPool->GetFlags(),
                        vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet))
        {
            SD_LOG(err,
                   "GuiContext descriptor pool requires "
                   "VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT");
            return false;
        }
    }

    m_window = window;
    m_descriptorPool = std::move(descriptorPool);
    m_colorFormat = colorFormat;

    IMGUI_CHECKVERSION();
    m_imgui = ImGui::CreateContext();
    if (m_imgui == nullptr)
    {
        SD_LOG(err, "ImGui::CreateContext failed");
        Shutdown();
        return false;
    }
    m_implot = ImPlot::CreateContext();
    if (m_implot == nullptr)
    {
        SD_LOG(err, "ImPlot::CreateContext failed");
        Shutdown();
        return false;
    }

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    const float displayScale = std::max(m_window->GetDisplayScale(), 1.0f);
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(displayScale);
    style.FontScaleDpi = displayScale;

    if (!ImGui_ImplSDL3_InitForVulkan(m_window->GetHandle()))
    {
        SD_LOG(err, "ImGui_ImplSDL3_InitForVulkan failed");
        Shutdown();
        return false;
    }

    if (!ImGui_ImplVulkan_LoadFunctions(
            static_cast<uint32_t>(device->GetApiVersion()),
            [](const char* functionName, void* userData) -> PFN_vkVoidFunction
            {
                return static_cast<VulkanInstance*>(userData)->GetProcAddr(functionName);
            },
            instance))
    {
        SD_LOG(err, "ImGui_ImplVulkan_LoadFunctions failed");
        Shutdown();
        return false;
    }

    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.ApiVersion = static_cast<uint32_t>(device->GetApiVersion());
    initInfo.Instance = static_cast<VkInstance>(instance->GetHandle());
    initInfo.PhysicalDevice = static_cast<VkPhysicalDevice>(device->GetPhysicalDevice());
    initInfo.Device = static_cast<VkDevice>(device->GetHandle());
    initInfo.QueueFamily = device->GetQueueFamilyIndex(VulkanQueueFamily::Graphics);
    initInfo.Queue = device->GetQueue(VulkanQueueFamily::Graphics)->GetHandle();
    if (m_descriptorPool)
    {
        initInfo.DescriptorPool = static_cast<VkDescriptorPool>(m_descriptorPool->GetHandle());
    }
    else
    {
        initInfo.DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE;
    }
    // Vertex/index buffer ring and texture-destroy delay: frames in flight, not swapchain images.
    initInfo.MinImageCount = frameLag;
    initInfo.ImageCount = frameLag;

    initInfo.UseDynamicRendering = true;
    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats =
        reinterpret_cast<const VkFormat*>(&m_colorFormat);

    initInfo.CheckVkResultFn = &CheckVkResult;

    if (!ImGui_ImplVulkan_Init(&initInfo))
    {
        SD_LOG(err, "ImGui_ImplVulkan_Init failed");
        Shutdown();
        return false;
    }

    return true;
}

void GuiContext::Shutdown()
{
    if (m_imgui == nullptr)
    {
        m_descriptorPool.reset();
        m_window = nullptr;
        m_colorFormat = vk::Format::eUndefined;
        return;
    }

    MakeCurrent();
    ImGuiIO& io = ImGui::GetIO();

    if ((io.BackendRendererUserData != nullptr) && (m_window != nullptr) &&
        (m_window->GetDevice() != nullptr))
    {
        try
        {
            m_window->WaitIdle();
        }
        catch (const std::exception& exception)
        {
            SD_LOG(err, "GuiContext::Shutdown WaitIdle failed: {}", exception.what());
        }
    }

    if (io.BackendRendererUserData != nullptr)
    {
        ImGui_ImplVulkan_Shutdown();
    }
    if (io.BackendPlatformUserData != nullptr)
    {
        ImGui_ImplSDL3_Shutdown();
    }

    if (m_implot != nullptr)
    {
        ImPlot::DestroyContext(m_implot);
        m_implot = nullptr;
    }
    ImGui::DestroyContext(m_imgui);
    m_imgui = nullptr;
    m_descriptorPool.reset();
    m_window = nullptr;
    m_colorFormat = vk::Format::eUndefined;
}

void GuiContext::MakeCurrent() const
{
    assert(m_imgui != nullptr);
    ImGui::SetCurrentContext(m_imgui);
    if (m_implot != nullptr)
    {
        ImPlot::SetCurrentContext(m_implot);
    }
}

ImGuiIO& GuiContext::GetIO() const
{
    MakeCurrent();
    return ImGui::GetIO();
}

ImGuiStyle& GuiContext::GetStyle() const
{
    MakeCurrent();
    return ImGui::GetStyle();
}

ImFont* GuiContext::AddFontFromFileTTF(const char* filename, float sizePixels,
                                       const ImFontConfig* fontCfg, const ImWchar* glyphRanges)
{
    assert(filename != nullptr);
    MakeCurrent();
    ImFont* font =
        ImGui::GetIO().Fonts->AddFontFromFileTTF(filename, sizePixels, fontCfg, glyphRanges);
    if (font == nullptr)
    {
        SD_LOG(err, "GuiContext::AddFontFromFileTTF failed to load '{}'", filename);
    }
    return font;
}

bool GuiContext::ProcessEvent(const SDL_Event& event)
{
    if (m_imgui == nullptr)
    {
        return false;
    }

    MakeCurrent();
    return ImGui_ImplSDL3_ProcessEvent(&event);
}

void GuiContext::NewFrame()
{
    MakeCurrent();
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void GuiContext::Render(VulkanCommandBuffer* commandBuffer)
{
    assert(commandBuffer != nullptr);
    MakeCurrent();

    ImGui::Render();
    ImDrawData* drawData = ImGui::GetDrawData();
    if ((drawData == nullptr) || (drawData->DisplaySize.x <= 0.0f) ||
        (drawData->DisplaySize.y <= 0.0f))
    {
        return;
    }

    ImGui_ImplVulkan_RenderDrawData(drawData,
                                    static_cast<VkCommandBuffer>(commandBuffer->GetHandle()));
}

void GuiContext::CreateMainPipeline()
{
    ImGui_ImplVulkan_PipelineInfo pipelineInfo = {};
    pipelineInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    pipelineInfo.PipelineRenderingCreateInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    pipelineInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    pipelineInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats =
        reinterpret_cast<const VkFormat*>(&m_colorFormat);
    ImGui_ImplVulkan_CreateMainPipeline(&pipelineInfo);
}

void GuiContext::OnRenderTargetChanged(vk::Format colorFormat)
{
    if (m_imgui == nullptr)
    {
        return;
    }
    if (colorFormat == vk::Format::eUndefined)
    {
        SD_LOG(err, "GuiContext::OnRenderTargetChanged requires a valid colorFormat");
        return;
    }
    if (colorFormat == m_colorFormat)
    {
        return;
    }

    MakeCurrent();
    m_colorFormat = colorFormat;
    CreateMainPipeline();
}

void GuiContext::OnSwapchainRecreated()
{
    if ((m_imgui == nullptr) || (m_window == nullptr))
    {
        return;
    }

    const uint32_t frameLag = m_window->GetMaxFrameLag();
    if (frameLag < 2)
    {
        SD_LOG(err, "GuiContext::OnSwapchainRecreated requires MaxFrameLag >= 2 (got {})",
               frameLag);
        return;
    }

    MakeCurrent();
    // No-op while MaxFrameLag is unchanged. Safe after CreateSwapchain's WaitIdle.
    ImGui_ImplVulkan_SetMinImageCount(frameLag);
}

} // namespace sd
