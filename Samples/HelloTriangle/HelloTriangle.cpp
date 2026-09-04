#include "MainWindow.h"

#include <SweetDream/Core/API/Vulkan/VulkanBuffer.h>
#include <SweetDream/Core/API/Vulkan/VulkanCommand.h>
#include <SweetDream/Core/API/Vulkan/VulkanDescriptor.h>
#include <SweetDream/Core/API/Vulkan/VulkanDevice.h>
#include <SweetDream/Core/API/Vulkan/VulkanImage.h>
#include <SweetDream/Core/API/Vulkan/VulkanInstance.h>
#include <SweetDream/Core/API/Vulkan/VulkanPipeline.h>
#include <SweetDream/Core/API/Vulkan/VulkanQueue.h>
#include <SweetDream/Core/API/Vulkan/VulkanSwapchain.h>
#include <SweetDream/Core/API/Vulkan/VulkanUploader.h>
#include <SweetDream/Core/IO/Logging.h>
#include <SweetDream/Gui/GuiContext.h>
#include <SweetDream/Platform/Application.h>
#include <SweetDream/Shader/GLSLCompiler.h>
#if !defined(NDEBUG)
#include <SweetDream/Shader/SpvContext.h>
#endif

#include <rad/Math/Matrix.h>
#include <rad/Math/Vector.h>

#include <Eigen/Geometry>

#include <imgui.h>
#include <implot.h>

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_main.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <numbers>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{

struct TriangleVertex
{
    rad::Vector2f position;
    rad::Vector3f color;
};

struct alignas(16) TriangleUniform
{
    rad::Matrix4f transform;
};

// Equilateral triangle centered at the origin; vertices lie on this circle.
constexpr float TriangleCircumradius = 0.75f;

[[nodiscard]] rad::Matrix4f Project(const rad::Matrix4f& matrix, float aspect)
{
    rad::Matrix4f projection = rad::Matrix4f::Identity();
    projection(0, 0) = aspect > 1.0f ? 1.0f / aspect : 1.0f;
    projection(1, 1) = aspect > 1.0f ? -1.0f : -aspect;
    projection(2, 2) = 0.5f;
    projection(2, 3) = 0.5f;
    return matrix * projection;
}

[[nodiscard]] rad::Matrix4f Rotate(const rad::Matrix4f& matrix, float angleRadians,
                                   const rad::Vector3f& axis)
{
    return matrix * Eigen::Isometry3f(Eigen::AngleAxisf(angleRadians, axis.normalized()))
                        .matrix();
}

void DrawTriangleCircumcircle()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 size = viewport->Size;
    if ((size.x <= 0.0f) || (size.y <= 0.0f))
    {
        return;
    }

    const float radius =
        TriangleCircumradius * 0.5f * ((size.x < size.y) ? size.x : size.y);
    // num_segments == 0 uses a 48-sample LUT. Tessellate from radius (already min viewport side).
    int numSegments = static_cast<int>(radius);
    if (numSegments < 48)
    {
        numSegments = 48;
    }
    ImGui::GetBackgroundDrawList()->AddCircle(viewport->GetCenter(), radius, IM_COL32_WHITE,
                                               numSegments, 2.0f);
}

constexpr const char* TriangleVertexShader = R"glsl(
#version 460

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;

layout(set = 0, binding = 0) uniform Transform
{
    mat4 matrix;
} transform;

layout(location = 0) out vec3 vertexColor;

void main()
{
    gl_Position = transform.matrix * vec4(inPosition, 0.0, 1.0);
    vertexColor = inColor;
}
)glsl";

constexpr const char* TriangleFragmentShader = R"glsl(
#version 460

layout(location = 0) in vec3 vertexColor;
layout(location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(vertexColor, 1.0);
}
)glsl";

} // namespace

class HelloTriangle final : public sd::Application
{
protected:
    bool OnInitialize() override
    {
        m_window = rad::Ref<MainWindow>{new MainWindow};
        if (!m_window->Create("HelloTriangle", 1600, 900,
                              SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY))
        {
            throw std::runtime_error("Failed to create the application window");
        }
        if (!RegisterEventHandler(m_window))
        {
            throw std::runtime_error("Failed to register the application window");
        }

        rad::Ref<sd::VulkanInstance> instance = sd::VulkanInstance::Create(
            "HelloTriangle", VK_MAKE_API_VERSION(0, 1, 0, 0), "SweetDream",
            VK_MAKE_API_VERSION(0, 0, 0, 0), GetVulkanInstanceExtensions());
        if (!instance)
        {
            throw std::runtime_error("Failed to create Vulkan instance");
        }
        if (!m_window->CreateVulkanSurface(instance))
        {
            throw std::runtime_error("Failed to create the window Vulkan surface");
        }

        const sd::VulkanVersion apiVersion = instance->GetApiVersion();
        SD_LOG(info, "Instance API version: {}", apiVersion.ToString());

        const auto& physicalDevices = instance->GetPhysicalDevices();
        if (physicalDevices.empty())
        {
            throw std::runtime_error("No Vulkan physical devices are available");
        }

        for (std::size_t i = 0; i < physicalDevices.size(); ++i)
        {
            const vk::PhysicalDeviceProperties properties =
                physicalDevices[i].getProperties(instance->GetDispatcher());
            SD_LOG(info, "Device #{}: {} (0x{:04X})", i, properties.deviceName.data(),
                   properties.deviceID);
        }

        vk::PhysicalDevice selectedDevice;
        for (const vk::PhysicalDevice& physicalDevice : physicalDevices)
        {
            if (m_window->CanPresent(physicalDevice))
            {
                selectedDevice = physicalDevice;
                break;
            }
        }
        if (!selectedDevice)
        {
            throw std::runtime_error(
                "No Vulkan physical device can present to the window surface");
        }

        rad::Ref<sd::VulkanDevice> device{
            new sd::VulkanDevice(instance, selectedDevice,
                                 m_window->GetVulkanSurface()->GetHandle())};
        if (!m_window->SetDevice(std::move(device)))
        {
            throw std::runtime_error("Failed to bind the Vulkan device to the window");
        }

        SD_LOG(info, "Logical device created on '{}'", m_window->GetDevice()->GetName());
        if (!m_window->CreateSwapchain())
        {
            throw std::runtime_error("Failed to create Vulkan swapchain");
        }

        CreateRenderingResources();
        CreateCommandBuffers();
        if (!m_gui.Init(m_window.get()))
        {
            throw std::runtime_error("Failed to initialize GuiContext");
        }
        SD_LOG(info, "Press F1 to show/hide the ImGui demo window");
        SD_LOG(info, "Press F2 to show/hide the ImPlot demo window");
        // Wake the event loop so the triangle keeps rotating without input.
        SetEventWaitTimeout(1);
        return true;
    }

    bool OnEvent(const SDL_Event& event) override
    {
        m_gui.ProcessEvent(event);

        const ImGuiIO& io = m_gui.GetIO();
        if ((event.type == SDL_EVENT_KEY_DOWN) && !event.key.repeat)
        {
            if (event.key.key == SDLK_F1)
            {
                m_showDemoWindow = !m_showDemoWindow;
            }
            if (event.key.key == SDLK_F2)
            {
                m_showPlotDemoWindow = !m_showPlotDemoWindow;
            }
        }

        if (io.WantCaptureMouse &&
            ((event.type == SDL_EVENT_MOUSE_MOTION) || (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) ||
             (event.type == SDL_EVENT_MOUSE_BUTTON_UP) || (event.type == SDL_EVENT_MOUSE_WHEEL)))
        {
            return true;
        }
        if (io.WantCaptureKeyboard &&
            ((event.type == SDL_EVENT_KEY_DOWN) || (event.type == SDL_EVENT_KEY_UP) ||
             (event.type == SDL_EVENT_TEXT_INPUT) || (event.type == SDL_EVENT_TEXT_EDITING)))
        {
            return true;
        }
        return false;
    }

    void OnFrame() override
    {
        if (!m_window)
        {
            return;
        }
        if (m_window->IsCloseRequested())
        {
            RequestExit();
            return;
        }

        sd::FrameStatus frameStatus = m_window->AcquireNextFrame();
        while ((frameStatus == sd::FrameStatus::OutOfDate) ||
               (frameStatus == sd::FrameStatus::SurfaceLost))
        {
            if (!RecreateSwapchainResources())
            {
                return;
            }
            frameStatus = m_window->AcquireNextFrame();
        }
        if (frameStatus == sd::FrameStatus::Skip)
        {
            return;
        }
        if (frameStatus != sd::FrameStatus::Ready)
        {
            SD_LOG(err, "HelloTriangle AcquireNextFrame failed");
            RequestExit(EXIT_FAILURE);
            return;
        }

        sd::VulkanImage* image = m_window->GetSwapchainImage();
        sd::VulkanImageView* imageView = m_window->GetSwapchainImageView();
        const uint32_t frameSlotIndex = m_window->GetFrameSlotIndex();
        sd::VulkanCommandBuffer* commandBuffer = m_commandBuffers[frameSlotIndex].get();
        const vk::Extent2D extent = m_window->GetSwapchain()->GetImageExtent();
        UpdateUniformBuffer(frameSlotIndex, extent);
        const vk::ImageSubresourceRange colorRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0,
                                                   1};

        m_gui.NewFrame();
        DrawTriangleCircumcircle();
        if (m_showDemoWindow)
        {
            ImGui::ShowDemoWindow(&m_showDemoWindow);
        }
        if (m_showPlotDemoWindow)
        {
            ImPlot::ShowDemoWindow(&m_showPlotDemoWindow);
        }

        commandBuffer->Reset();
        commandBuffer->Begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

        commandBuffer->ImageMemoryBarrier(
            image->GetHandle(), colorRange, vk::ImageLayout::eUndefined,
            vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eNone,
            vk::AccessFlagBits2::eNone, vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::AccessFlagBits2::eColorAttachmentWrite);

        sd::VulkanRenderingAttachmentInfo colorAttachment;
        colorAttachment.imageView = imageView;
        colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
        colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
        colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
        colorAttachment.clearValue.color =
            vk::ClearColorValue(std::array<float, 4>{0.15f, 0.25f, 0.45f, 1.0f});

        const vk::Rect2D renderArea{{0, 0}, extent};
        commandBuffer->BeginRendering({colorAttachment}, nullptr, nullptr, &renderArea);

        const vk::Viewport viewport{0.0f, 0.0f, static_cast<float>(extent.width),
                                    static_cast<float>(extent.height), 0.0f, 1.0f};
        const vk::Buffer vertexBuffer = m_vertexBuffer->GetHandle();
        constexpr vk::DeviceSize vertexBufferOffset = 0;
        const vk::DescriptorSet descriptorSet = m_descriptorSets[frameSlotIndex]->GetHandle();

        commandBuffer->BindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline->GetHandle());
        commandBuffer->BindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                          m_pipelineLayout->GetHandle(), 0, descriptorSet);
        commandBuffer->BindVertexBuffers(0, vertexBuffer, vertexBufferOffset);
        commandBuffer->BindIndexBuffer(m_indexBuffer->GetHandle(), 0, vk::IndexType::eUint32);
        commandBuffer->SetViewportWithCount(viewport);
        commandBuffer->SetScissorWithCount(renderArea);
        commandBuffer->DrawIndexed(m_indexCount, 1, 0, 0, 0);

        m_gui.Render(commandBuffer);
        commandBuffer->EndRendering();

        commandBuffer->ImageMemoryBarrier(
            image->GetHandle(), colorRange, vk::ImageLayout::eColorAttachmentOptimal,
            vk::ImageLayout::ePresentSrcKHR, vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::AccessFlagBits2::eColorAttachmentWrite, vk::PipelineStageFlagBits2::eNone,
            vk::AccessFlagBits2::eNone);

        commandBuffer->End();

        const vk::PipelineStageFlags waitStage =
            vk::PipelineStageFlagBits::eColorAttachmentOutput;
        m_window->GetDevice()
            ->GetQueue(sd::VulkanQueueFamily::Graphics)
            ->Submit(commandBuffer, m_window->GetSwapchainImageAcquiredSemaphore(), waitStage,
                     m_window->GetRenderCompleteSemaphore(), m_window->GetFrameFence());

        if (!m_window->Present())
        {
            SD_LOG(err, "HelloTriangle Present failed");
            RequestExit(EXIT_FAILURE);
        }
    }

    void OnShutdown() override
    {
        if (m_window)
        {
            m_window->WaitIdle();
            m_gui.Shutdown();
            DestroyRenderingResources();
            m_window->Destroy();
            m_window.reset();
        }
    }

private:
    [[nodiscard]] bool RecreateSwapchainResources()
    {
        if (!m_window || !m_window->CreateSwapchain())
        {
            return false;
        }

        m_gui.OnSwapchainRecreated();
        const vk::Format format = m_window->GetSwapchain()->GetImageFormat();
        if (m_pipelineColorFormat != format)
        {
            CreateTrianglePipeline();
        }
        m_gui.OnRenderTargetChanged(format);
        return true;
    }

    void CreateRenderingResources()
    {
        sd::VulkanDevice* device = m_window->GetDevice();

        sd::GLSLCompiler compiler;
        auto vertexSpirv = compiler.CompileToSpv(
            vk::ShaderStageFlagBits::eVertex, "HelloTriangle.vert", TriangleVertexShader);
        if (!vertexSpirv.has_value())
        {
            throw std::runtime_error("Vertex shader compilation failed: " +
                                     vertexSpirv.assume_error().message);
        }
        auto fragmentSpirv = compiler.CompileToSpv(vk::ShaderStageFlagBits::eFragment,
                                                   "HelloTriangle.frag",
                                                   TriangleFragmentShader);
        if (!fragmentSpirv.has_value())
        {
            throw std::runtime_error("Fragment shader compilation failed: " +
                                     fragmentSpirv.assume_error().message);
        }

#if !defined(NDEBUG)
        sd::SpvContext spvContext;
        const auto inspectSpirv = [&spvContext](std::string_view name,
                                                const std::vector<uint32_t>& spirv)
        {
            constexpr sd::SpvDisassembleOptions options =
                sd::SpvDisassembleOptions::Indent | sd::SpvDisassembleOptions::FriendlyNames;
            auto disassembly = spvContext.ValidateAndDisassemble(spirv, options);
            if (!disassembly.has_value())
            {
                throw std::runtime_error(std::string{name} +
                                         " SPIR-V validation/disassembly failed: " +
                                         disassembly.assume_error().message);
            }
            SD_LOG(debug, "{} SPIR-V:\n{}", name, disassembly.assume_value());
        };
        inspectSpirv("Vertex shader", vertexSpirv.assume_value());
        inspectSpirv("Fragment shader", fragmentSpirv.assume_value());
#endif

        m_vertexShader = device->CreateShaderModule(vertexSpirv.assume_value());
        m_fragmentShader = device->CreateShaderModule(fragmentSpirv.assume_value());

        // Equilateral triangle centered at the origin; vertices lie on this circle.
        constexpr float circumradius = TriangleCircumradius;
        constexpr float halfHeight = circumradius * 0.5f;
        constexpr float halfWidth = circumradius * std::numbers::sqrt3_v<float> * 0.5f;
        const std::array<TriangleVertex, 3> vertices = {{
            {rad::Vector2f{0.0f, circumradius}, rad::Vector3f{1.0f, 0.0f, 0.0f}},
            {rad::Vector2f{-halfWidth, -halfHeight}, rad::Vector3f{0.0f, 1.0f, 0.0f}},
            {rad::Vector2f{halfWidth, -halfHeight}, rad::Vector3f{0.0f, 0.0f, 1.0f}},
        }};
        constexpr std::array<uint32_t, 3> indices = {0, 1, 2};

        rad::Ref<sd::VulkanUploader> uploader = device->CreateUploader();
        m_vertexBuffer = device->CreateVertexBuffer(sizeof(vertices));
        uploader->UploadBuffer(m_vertexBuffer.get(), vertices.data(), sizeof(vertices));
        m_indexBuffer = device->CreateIndexBuffer(sizeof(indices));
        uploader->UploadBuffer(m_indexBuffer.get(), indices.data(), sizeof(indices));
        uploader->SubmitAndWait();
        m_indexCount = static_cast<uint32_t>(indices.size());

        const vk::DescriptorSetLayoutBinding uniformBinding =
            sd::MakeDescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer,
                                               vk::ShaderStageFlagBits::eVertex);
        m_descriptorSetLayout = device->CreateDescriptorSetLayout(uniformBinding);

        m_pipelineLayout = device->CreatePipelineLayout(m_descriptorSetLayout->GetHandle());

        const uint32_t frameCount = m_window->GetMaxFrameLag();
        const vk::DescriptorPoolSize poolSize{vk::DescriptorType::eUniformBuffer, frameCount};
        m_descriptorPool = device->CreateDescriptorPool(frameCount, poolSize);

        const std::vector<vk::DescriptorSetLayout> descriptorLayouts(
            frameCount, m_descriptorSetLayout->GetHandle());
        m_descriptorSets = m_descriptorPool->Allocate(descriptorLayouts);

        const vk::DeviceSize uniformAlignment =
            device->GetProperties().limits.minUniformBufferOffsetAlignment;
        m_uniformBufferStride = (sizeof(TriangleUniform) + uniformAlignment - 1) /
                                uniformAlignment * uniformAlignment;
        m_uniformBuffer = device->CreateUniformBuffer(m_uniformBufferStride * frameCount);
        m_uniformBufferMapping = static_cast<std::byte*>(m_uniformBuffer->GetMappedData());
        if (m_uniformBufferMapping == nullptr)
        {
            throw std::runtime_error("Uniform buffer was not persistently mapped");
        }

        for (uint32_t frameIndex = 0; frameIndex < frameCount; ++frameIndex)
        {
            m_descriptorSets[frameIndex]->WriteBuffer(
                0, vk::DescriptorType::eUniformBuffer, m_uniformBuffer.get(),
                m_uniformBufferStride * frameIndex, sizeof(TriangleUniform));
        }

        CreateTrianglePipeline();
        m_startTime = std::chrono::steady_clock::now();
    }

    void CreateTrianglePipeline()
    {
        sd::VulkanGraphicsPipelineCreateInfo pipelineInfo;

        pipelineInfo.m_stages.emplace_back(vk::ShaderStageFlagBits::eVertex, m_vertexShader);
        pipelineInfo.m_stages.emplace_back(vk::ShaderStageFlagBits::eFragment,
                                           m_fragmentShader);

        pipelineInfo.m_vertexInputState.bindings = {
            {0, static_cast<uint32_t>(sizeof(TriangleVertex)), vk::VertexInputRate::eVertex},
        };
        pipelineInfo.m_vertexInputState.attributes = {
            {0, 0, vk::Format::eR32G32Sfloat,
             static_cast<uint32_t>(offsetof(TriangleVertex, position))},
            {1, 0, vk::Format::eR32G32B32Sfloat,
             static_cast<uint32_t>(offsetof(TriangleVertex, color))},
        };

        pipelineInfo.m_colorBlendState.attachments = {
            sd::MakeOpaqueColorBlendAttachmentState()};

        pipelineInfo.m_layout = m_pipelineLayout;
        m_pipelineColorFormat = m_window->GetSwapchain()->GetImageFormat();
        pipelineInfo.m_colorAttachmentFormats = {m_pipelineColorFormat};
        m_pipeline = m_window->GetDevice()->CreateGraphicsPipeline(pipelineInfo);
    }

    void UpdateUniformBuffer(uint32_t frameIndex, vk::Extent2D extent)
    {
        const float elapsedSeconds =
            std::chrono::duration<float>(std::chrono::steady_clock::now() - m_startTime)
                .count();
        const float aspect =
            static_cast<float>(extent.width) / static_cast<float>(extent.height);
        const TriangleUniform uniform{
            Rotate(Project(rad::Matrix4f::Identity(), aspect), elapsedSeconds,
                   -rad::Vector3f::UnitZ())};

        std::memcpy(m_uniformBufferMapping + m_uniformBufferStride * frameIndex, &uniform,
                    sizeof(uniform));
    }

    void CreateCommandBuffers()
    {
        sd::VulkanDevice* device = m_window->GetDevice();
        m_commandPool = device->CreateCommandPool(
            sd::VulkanQueueFamily::Graphics, vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
        m_commandBuffers = m_commandPool->AllocateCommandBuffers(
            vk::CommandBufferLevel::ePrimary, m_window->GetMaxFrameLag());
        SD_LOG(info, "Allocated {} command buffers", m_commandBuffers.size());
    }

    void DestroyRenderingResources()
    {
        m_commandBuffers.clear();
        m_commandPool.reset();
        m_pipeline.reset();
        m_pipelineColorFormat = vk::Format::eUndefined;
        m_pipelineLayout.reset();
        m_descriptorSets.clear();
        m_descriptorPool.reset();
        m_descriptorSetLayout.reset();
        m_uniformBufferMapping = nullptr;
        m_uniformBuffer.reset();
        m_indexCount = 0;
        m_indexBuffer.reset();
        m_vertexBuffer.reset();
        m_fragmentShader.reset();
        m_vertexShader.reset();
    }

    rad::Ref<MainWindow> m_window;
    rad::Ref<sd::VulkanShaderModule> m_vertexShader;
    rad::Ref<sd::VulkanShaderModule> m_fragmentShader;
    rad::Ref<sd::VulkanDescriptorSetLayout> m_descriptorSetLayout;
    rad::Ref<sd::VulkanDescriptorPool> m_descriptorPool;
    std::vector<rad::Ref<sd::VulkanDescriptorSet>> m_descriptorSets;
    rad::Ref<sd::VulkanPipelineLayout> m_pipelineLayout;
    rad::Ref<sd::VulkanGraphicsPipeline> m_pipeline;
    vk::Format m_pipelineColorFormat = vk::Format::eUndefined;
    rad::Ref<sd::VulkanBuffer> m_vertexBuffer;
    rad::Ref<sd::VulkanBuffer> m_indexBuffer;
    uint32_t m_indexCount = 0;
    rad::Ref<sd::VulkanBuffer> m_uniformBuffer;
    std::byte* m_uniformBufferMapping = nullptr;
    vk::DeviceSize m_uniformBufferStride = 0;
    rad::Ref<sd::VulkanCommandPool> m_commandPool;
    std::vector<rad::Ref<sd::VulkanCommandBuffer>> m_commandBuffers;
    std::chrono::steady_clock::time_point m_startTime;
    sd::GuiContext m_gui;
    bool m_showDemoWindow = false;
    bool m_showPlotDemoWindow = false;
}; // class HelloTriangle

#if defined(SDL_MAIN_USE_CALLBACKS)

SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv)
{
    if (appstate == nullptr)
    {
        return SDL_APP_FAILURE;
    }

    auto* application = new (std::nothrow) HelloTriangle;
    *appstate = application;
    if ((application == nullptr) || !application->Init(argc, argv))
    {
        return SDL_APP_FAILURE;
    }
    return application->IsRunning() ? SDL_APP_CONTINUE : SDL_APP_SUCCESS;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event)
{
    auto* application = static_cast<HelloTriangle*>(appstate);
    if (application->IsRunning())
    {
        application->DispatchEvent(*event);
    }
    return application->IsRunning() ? SDL_APP_CONTINUE : SDL_APP_SUCCESS;
}

SDL_AppResult SDL_AppIterate(void* appstate)
{
    auto* application = static_cast<HelloTriangle*>(appstate);
    if (application->IsRunning())
    {
        application->DispatchFrame();
    }
    return application->IsRunning() ? SDL_APP_CONTINUE : SDL_APP_SUCCESS;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result)
{
    std::unique_ptr<HelloTriangle> application{static_cast<HelloTriangle*>(appstate)};
    if (!application)
    {
        return;
    }

    const int exitCode = (result == SDL_APP_FAILURE) ? EXIT_FAILURE : application->GetExitCode();
    application->RequestExit(exitCode);
    application->Shutdown();
}

#else

int main(int argc, char** argv)
{
    return HelloTriangle{}.Run(argc, argv);
}

#endif
