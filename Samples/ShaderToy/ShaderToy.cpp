#include "ShaderToyRenderer.h"

#include <SweetDream/GPU/Vulkan/VulkanQueue.h>
#include <SweetDream/Core/IO/Logging.h>
#include <SweetDream/Gui/VulkanWindow.h>
#include <SweetDream/Platform/Application.h>

#include <rad/Application.h>

#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace
{

void CheckGpuRequirements(vk::PhysicalDevice physicalDevice,
                          const vk::detail::DispatchLoaderDynamic& dispatcher)
{
    const auto properties = physicalDevice.getProperties(dispatcher);
    if (properties.apiVersion < VK_API_VERSION_1_3)
    {
        throw std::runtime_error("Selected GPU requires Vulkan 1.3 or newer");
    }
    vk::PhysicalDeviceVulkan13Features vulkan13;
    vk::PhysicalDeviceFeatures2 features;
    features.pNext = &vulkan13;
    physicalDevice.getFeatures2(&features, dispatcher);
    if (!vulkan13.dynamicRendering)
    {
        throw std::runtime_error("Selected GPU does not support dynamicRendering");
    }
    if (!vulkan13.synchronization2)
    {
        throw std::runtime_error("Selected GPU does not support synchronization2");
    }
    const auto extensions = physicalDevice.enumerateDeviceExtensionProperties(nullptr, dispatcher);
    if (std::none_of(extensions.begin(), extensions.end(), [](const auto& extension)
                    { return std::string_view(extension.extensionName.data()) ==
                             VK_KHR_SWAPCHAIN_EXTENSION_NAME; }))
    {
        throw std::runtime_error("Selected GPU does not support VK_KHR_swapchain");
    }
}

class ShaderToy final : public sd::Application
{
public:
    ShaderToy(shadertoy::CommandLineOptions options, shadertoy::EffectConfig config,
              shadertoy::CompiledShaders shaders) :
        m_options(std::move(options)), m_config(std::move(config)), m_shaders(std::move(shaders))
    {
    }

protected:
    bool OnInitialize() override
    {
        m_window = rad::Ref<sd::VulkanWindow>{new sd::VulkanWindow};
        if (!m_window->Create("ShaderToy", m_options.width, m_options.height, SDL_WINDOW_RESIZABLE) ||
            !RegisterEventHandler(m_window))
        {
            throw std::runtime_error("Failed to create/register ShaderToy window");
        }
        auto instance = sd::VulkanInstance::Create(
            "ShaderToy", VK_MAKE_API_VERSION(0, 1, 0, 0), "SweetDream",
            VK_MAKE_API_VERSION(0, 0, 0, 0), GetVulkanInstanceExtensions());
        if (!instance || !m_window->CreateVulkanSurface(instance))
        {
            throw std::runtime_error("Failed to create Vulkan instance/surface");
        }
        const auto& physicalDevices = instance->GetPhysicalDevices();
        if (physicalDevices.empty())
        {
            throw std::runtime_error("No Vulkan GPUs are available");
        }
        for (std::size_t i = 0; i < physicalDevices.size(); ++i)
        {
            const auto properties = physicalDevices[i].getProperties(instance->GetDispatcher());
            SD_LOG(info, "GPU #{}: {}", i, properties.deviceName.data());
        }
        if (m_options.gpuIndex >= physicalDevices.size())
        {
            throw std::invalid_argument("GPU index " + std::to_string(m_options.gpuIndex) +
                                        " is out of range [0, " +
                                        std::to_string(physicalDevices.size() - 1) + "]");
        }
        const vk::PhysicalDevice selected = physicalDevices[m_options.gpuIndex];
        SD_LOG(info, "Selected GPU #{}", m_options.gpuIndex);
        CheckGpuRequirements(selected, instance->GetDispatcher());
        if (!m_window->CanPresent(selected))
        {
            throw std::runtime_error("Selected GPU cannot present to the window");
        }
        rad::Ref<sd::VulkanDevice> device{new sd::VulkanDevice(
            instance, selected, m_window->GetVulkanSurface()->GetHandle())};
        if (!m_window->SetDevice(device))
        {
            throw std::runtime_error("Failed to bind the Vulkan device");
        }
        const vk::Result swapchainResult = m_window->CreateSwapchain();
        if (swapchainResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to create Vulkan swapchain: " +
                                     vk::to_string(swapchainResult));
        }
        SD_LOG(info, "ShaderToy device: {}", device->GetName());
        m_pool = device->CreateCommandPool(
            sd::VulkanQueueFamily::Graphics, vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
        m_commands = m_pool->AllocateCommandBuffers(
            vk::CommandBufferLevel::ePrimary, m_window->GetMaxFrameLag());
        m_renderer = std::make_unique<shadertoy::Renderer>(
            device.get(), std::move(m_config), std::move(m_shaders), m_window->GetMaxFrameLag());
        ResizeRenderer();
        SetEventWaitTimeout(1);
        return true;
    }

    bool OnEvent(const SDL_Event& event) override
    {
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.windowID == m_window->GetID() &&
            event.key.key == SDLK_ESCAPE)
        {
            RequestExit();
        }
        if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST && event.window.windowID == m_window->GetID())
        {
            m_down = false;
            m_click = false;
        }
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP)
        {
            if (event.button.windowID == m_window->GetID() && event.button.button == SDL_BUTTON_LEFT)
            {
                UpdateMouse(event.button.x, event.button.y);
                m_down = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
                if (m_down)
                {
                    m_origin = m_position;
                    // Preserve click-coordinate signs at the bottom/left edge.
                    m_origin[0] = std::max(m_origin[0], 0.000001f);
                    m_origin[1] = std::max(m_origin[1], 0.000001f);
                    m_click = true;
                }
            }
        }
        if (event.type == SDL_EVENT_MOUSE_MOTION && event.motion.windowID == m_window->GetID() && m_down)
        {
            UpdateMouse(event.motion.x, event.motion.y);
        }
        return false;
    }

    void OnFrame() override
    {
        if (m_window->IsCloseRequested())
        {
            RequestExit();
            return;
        }
        auto status = m_window->AcquireNextFrame();
        if (status == sd::FrameStatus::OutOfDate || status == sd::FrameStatus::SurfaceLost)
        {
            const vk::Result result = m_window->CreateSwapchain();
            switch (result)
            {
            case vk::Result::eSuccess:
                ResizeRenderer();
                SetEventWaitTimeout(1);
                break;
            case vk::Result::eNotReady:
            case vk::Result::eErrorSurfaceLostKHR:
            case vk::Result::eErrorOutOfDateKHR:
                SetEventWaitTimeout(16);
                break;
            default:
                throw std::runtime_error("Failed to recreate swapchain: " + vk::to_string(result));
            }
            return;
        }
        if (status == sd::FrameStatus::Skip)
        {
            SetEventWaitTimeout(16);
            return;
        }
        if (status != sd::FrameStatus::Ready)
        {
            throw std::runtime_error("Failed to acquire ShaderToy frame");
        }
        SetEventWaitTimeout(1);
        const shadertoy::FrameData frame = BuildFrameData();
        const auto slot = m_window->GetFrameSlotIndex();
        auto* command = m_commands[slot].get();
        auto* image = m_window->GetSwapchainImage();
        constexpr vk::ImageSubresourceRange range{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
        command->Reset();
        command->Begin();
        command->ImageMemoryBarrier(
            image->GetHandle(), range, vk::ImageLayout::eUndefined,
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::AccessFlagBits2::eNone, vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::AccessFlagBits2::eColorAttachmentWrite);
        m_renderer->Render(command, m_window->GetSwapchainImageView(), slot, frame);
        command->ImageMemoryBarrier(
            image->GetHandle(), range, vk::ImageLayout::eColorAttachmentOptimal,
            vk::ImageLayout::ePresentSrcKHR, vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::AccessFlagBits2::eColorAttachmentWrite, vk::PipelineStageFlagBits2::eNone,
            vk::AccessFlagBits2::eNone);
        command->End();
        m_window->GetDevice()->GetQueue(sd::VulkanQueueFamily::Graphics)->Submit(
            command, m_window->GetSwapchainImageAcquiredSemaphore(),
            vk::PipelineStageFlagBits::eColorAttachmentOutput,
            m_window->GetRenderCompleteSemaphore(), m_window->GetFrameFence());
        if (!m_window->Present())
        {
            throw std::runtime_error("Failed to present ShaderToy frame");
        }
        m_click = false;
        ++m_totalFrames;
        UpdateFrameRate();
        if (m_options.frames != 0 && m_totalFrames >= m_options.frames)
        {
            SD_LOG(info, "Rendered {} frames", m_totalFrames);
            RequestExit();
        }
    }

    void OnShutdown() override
    {
        if (m_window)
        {
            m_window->WaitIdle();
            m_commands.clear();
            m_pool.reset();
            m_renderer.reset();
            m_window->Destroy();
            m_window.reset();
        }
    }

private:
    shadertoy::FrameData BuildFrameData()
    {
        shadertoy::FrameData frame;
        frame.index = m_frame++;
        frame.time =
            std::chrono::duration<float>(std::chrono::steady_clock::now() - m_start).count();
        frame.delta = frame.index == 0 ? 0.0f : frame.time - m_previousTime;
        frame.frameRate = m_frameRate;
        m_previousTime = frame.time;
        frame.mouse = {m_position[0], m_position[1],
                       m_down ? m_origin[0] : -m_origin[0],
                       m_click ? m_origin[1] : -m_origin[1]};
        const auto now = std::chrono::system_clock::now();
        const auto seconds = std::chrono::system_clock::to_time_t(now);
        const std::tm* date = std::localtime(&seconds);
        if (date == nullptr)
        {
            throw std::runtime_error("Failed to obtain local date");
        }
        const float secondsSinceMidnight =
            static_cast<float>(date->tm_hour * 3600 + date->tm_min * 60 + date->tm_sec) +
            std::chrono::duration<float>(now.time_since_epoch() % std::chrono::seconds(1)).count();
        frame.date = {static_cast<float>(date->tm_year + 1900), static_cast<float>(date->tm_mon),
                      static_cast<float>(date->tm_mday), secondsSinceMidnight};
        return frame;
    }

    void UpdateFrameRate()
    {
        ++m_frameRateFrames;
        const auto now = std::chrono::steady_clock::now();
        const auto elapsed = now - m_frameRateStart;
        if (elapsed >= std::chrono::milliseconds{500})
        {
            m_frameRate = static_cast<float>(m_frameRateFrames) /
                          std::chrono::duration<float>(elapsed).count();
            m_frameRateFrames = 0;
            m_frameRateStart = now;
        }
    }

    void ResizeRenderer()
    {
        // CreateSwapchain waits for the device before replacing dependent targets.
        const auto* swapchain = m_window->GetSwapchain();
        m_renderer->Resize(swapchain->GetImageExtent(), swapchain->GetImageFormat());
        m_frame = 0;
        m_start = std::chrono::steady_clock::now();
        m_previousTime = 0.0f;
        m_frameRateStart = m_start;
        m_frameRateFrames = 0;
        m_frameRate = 60.0f;
        const auto extent = swapchain->GetImageExtent();
        SD_LOG(info, "ShaderToy render size: {}x{}", extent.width, extent.height);
    }

    void UpdateMouse(float x, float y)
    {
        int width = 0, height = 0;
        if (!m_window->GetSize(&width, &height) || width <= 0 || height <= 0)
        {
            throw std::runtime_error("Failed to query mouse coordinate extent");
        }
        m_position = {x / static_cast<float>(width), 1.0f - y / static_cast<float>(height)};
    }

    shadertoy::CommandLineOptions m_options;
    shadertoy::EffectConfig m_config;
    shadertoy::CompiledShaders m_shaders;
    rad::Ref<sd::VulkanWindow> m_window;
    std::unique_ptr<shadertoy::Renderer> m_renderer;
    rad::Ref<sd::VulkanCommandPool> m_pool;
    std::vector<rad::Ref<sd::VulkanCommandBuffer>> m_commands;
    std::chrono::steady_clock::time_point m_start;
    std::chrono::steady_clock::time_point m_frameRateStart;
    uint32_t m_frame = 0;
    uint32_t m_totalFrames = 0;
    uint32_t m_frameRateFrames = 0;
    float m_previousTime = 0.0f;
    float m_frameRate = 60.0f;
    std::array<float, 2> m_position = {};
    std::array<float, 2> m_origin = {};
    bool m_down = false;
    bool m_click = false;
};

} // namespace

int main(int argc, char** argv)
{
    rad::Application::Instance().Init(argc, argv);
    try
    {
        const shadertoy::CommandLineOptions options =
            shadertoy::ParseCommandLineOptions(rad::Application::Instance().Arguments());
        if (options.help)
        {
            std::cout << "ShaderToy [--width N] [--height N] [--config passes.json] [--gpu-index N]\n"
                         "          [--validate] [--frames N] [--help]\n"
                         "--validate: validate JSON and compile shaders without creating a window\n"
                         "--frames: exit after N rendered frames (default: run until closed)\n";
            return EXIT_SUCCESS;
        }
        shadertoy::EffectConfig config = shadertoy::LoadEffectConfig(options.config);
        auto shaders = shadertoy::CompileShaders(config);
        if (options.validate)
        {
            SD_LOG(info, "Validated {} passes", config.passes.size());
            return EXIT_SUCCESS;
        }
        SDL_SetMainReady();
        return ShaderToy{options, std::move(config), std::move(shaders)}.Run(argc, argv);
    }
    catch (const std::exception& error)
    {
        SD_LOG(err, "ShaderToy: {}", error.what());
        return EXIT_FAILURE;
    }
}
