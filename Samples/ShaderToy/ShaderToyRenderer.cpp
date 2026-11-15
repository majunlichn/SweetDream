#include "ShaderToyRenderer.h"

#include <SweetDream/Core/API/Vulkan/VulkanQueue.h>
#include <SweetDream/Shader/GLSLCompiler.h>

#include <rad/IO/File.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>
#include <map>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace shadertoy
{
namespace
{

constexpr vk::Format BufferFormat = vk::Format::eR32G32B32A32Sfloat;
constexpr vk::ImageSubresourceRange ColorRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

struct alignas(16) Uniforms
{
    Float4 resolutionTime;
    float delta;
    int32_t frame;
    float frameRate;
    float sampleRate;
    Float4 mouse;
    Float4 date;
    std::array<Float4, 4> channelTime;
    std::array<Float4, 4> channelResolution;
};
static_assert(sizeof(Uniforms) == 192 && offsetof(Uniforms, channelTime) == 64 &&
              offsetof(Uniforms, channelResolution) == 128);

bool IsSrgb(vk::Format format)
{
    return format == vk::Format::eR8G8B8A8Srgb || format == vk::Format::eB8G8R8A8Srgb ||
           format == vk::Format::eA8B8G8R8SrgbPack32;
}

std::string ShaderPath(const rad::FilePath& path)
{
    std::string filename = rad::ToUtf8(path);
    std::replace(filename.begin(), filename.end(), '\\', '/');
    return filename;
}

void AppendShaderSource(std::string& source, const rad::FilePath& path)
{
    const auto text = rad::File::ReadAllText(path);
    if (!text)
    {
        throw std::runtime_error("Cannot read file: " + rad::ToUtf8(path));
    }
    source += "\n#line 1 \"" + ShaderPath(path) + "\"\n" + *text + "\n";
}

} // namespace

CompiledShaders CompileShaders(const EffectConfig& config)
{
    sd::GLSLCompiler compiler;
    compiler.SetTargetVulkanVersion(shaderc_env_version_vulkan_1_3);
    const rad::FilePath assetDirectory = GetAssetDirectory();
    auto vertex = compiler.CompileFileToSpv(vk::ShaderStageFlagBits::eVertex,
                                            assetDirectory / "ShaderToy.vert");
    if (!vertex)
    {
        throw std::runtime_error("Vertex shader: " + vertex.assume_error().message);
    }
    const rad::FilePath fragmentPath = assetDirectory / "ShaderToy.frag";
    const auto fragmentSource = rad::File::ReadAllText(fragmentPath);
    if (!fragmentSource)
    {
        throw std::runtime_error("Cannot read file: " + rad::ToUtf8(fragmentPath));
    }
    CompiledShaders shaders;
    shaders.vertex = std::move(vertex.assume_value());
    shaders.fragments.reserve(config.passes.size());
    for (std::size_t i = 0; i < config.passes.size(); ++i)
    {
        const PassConfig& pass = config.passes[i];
        std::string source = *fragmentSource;
        if (!config.common.empty())
        {
            source += "\n#include \"" + ShaderPath(config.common) + "\"\n";
        }
        AppendShaderSource(source, pass.shaderPath);
        const std::array<sd::GLSLMacro, 1> macros = {{
            {"RENDER_TO_TEXTURE", i + 1 == config.passes.size() ? "0" : "1"},
        }};
        auto fragment = compiler.CompileToSpv(
            vk::ShaderStageFlagBits::eFragment, rad::ToUtf8(pass.shaderPath), source, "main",
            macros, sd::GLSLCompileOptLevel::Performance);
        if (!fragment)
        {
            throw std::runtime_error(pass.name + ": " + fragment.assume_error().message);
        }
        shaders.fragments.push_back(std::move(fragment.assume_value()));
    }
    return shaders;
}

Renderer::Renderer(sd::VulkanDevice* device, EffectConfig config, CompiledShaders shaders,
                   uint32_t frameSlotCount) :
    m_device(device),
    m_config(std::move(config)),
    m_frameSlotCount(frameSlotCount)
{
    if (device == nullptr || frameSlotCount == 0 || shaders.fragments.size() != m_config.passes.size() ||
        shaders.vertex.empty() || shaders.fragments.empty())
    {
        throw std::invalid_argument("Renderer requires a device, frame slots and compiled passes");
    }
    const auto features = device->GetPhysicalDevice()
                              .getFormatProperties(BufferFormat, device->GetDispatcher())
                              .optimalTilingFeatures;
    const auto required = vk::FormatFeatureFlagBits::eColorAttachment |
                          vk::FormatFeatureFlagBits::eSampledImage |
                          vk::FormatFeatureFlagBits::eTransferDst;
    if ((features & required) != required)
    {
        throw std::runtime_error("RGBA32F Buffer attachments are unsupported on this device");
    }

    std::array<vk::DescriptorSetLayoutBinding, 5> bindings;
    bindings[0] = sd::MakeDescriptorSetLayoutBinding(
        0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eFragment);
    for (uint32_t i = 1; i < bindings.size(); ++i)
    {
        bindings[i] = sd::MakeDescriptorSetLayoutBinding(
            i, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment);
    }
    m_setLayout = device->CreateDescriptorSetLayout(bindings);
    m_layout = device->CreatePipelineLayout(m_setLayout->GetHandle());
    const uint32_t count = static_cast<uint32_t>(shaders.fragments.size()) * frameSlotCount;
    const std::array<vk::DescriptorPoolSize, 2> poolSizes = {{
        {vk::DescriptorType::eUniformBuffer, count},
        {vk::DescriptorType::eCombinedImageSampler, count * 4},
    }};
    m_descriptorPool = device->CreateDescriptorPool(count, poolSizes);
    const auto alignment = device->GetProperties().limits.minUniformBufferOffsetAlignment;
    m_uniformStride = (sizeof(Uniforms) + alignment - 1) / alignment * alignment;
    m_uniformBuffer = device->CreateUniformBuffer(m_uniformStride * count);
    m_uniformMapping = static_cast<std::byte*>(m_uniformBuffer->GetMappedData());
    if (m_uniformMapping == nullptr)
    {
        throw std::runtime_error("Uniform buffer was not persistently mapped");
    }
    m_vertex = device->CreateShaderModule(shaders.vertex);
    m_passes.resize(shaders.fragments.size());
    std::map<std::string, std::size_t> passIndices;
    for (std::size_t i = 0; i < m_config.passes.size(); ++i)
    {
        passIndices.emplace(m_config.passes[i].name, i);
    }
    std::map<std::tuple<rad::FilePath, bool, bool>, Texture> textures;
    std::map<std::pair<TextureFilter, bool>, rad::Ref<sd::VulkanSampler>> samplers;
    for (std::size_t i = 0; i < shaders.fragments.size(); ++i)
    {
        auto& pass = m_passes[i];
        pass.shader = device->CreateShaderModule(shaders.fragments[i]);
        pass.descriptorSets = m_descriptorPool->Allocate(
            std::vector<vk::DescriptorSetLayout>(frameSlotCount, m_setLayout->GetHandle()));
        for (uint32_t slot = 0; slot < frameSlotCount; ++slot)
        {
            pass.descriptorSets[slot]->WriteBuffer(
                0, vk::DescriptorType::eUniformBuffer, m_uniformBuffer.get(),
                m_uniformStride * (i * frameSlotCount + slot), sizeof(Uniforms));
        }
        const auto& configuredInputs = m_config.passes[i].inputs;
        for (uint32_t channel = 0; channel < pass.inputs.size(); ++channel)
        {
            const InputChannelConfig* input =
                channel < configuredInputs.size() && configuredInputs[channel]
                    ? &*configuredInputs[channel]
                    : nullptr;
            InputChannel& channelInput = pass.inputs[channel];
            vk::FormatFeatureFlags samplingFeatures = features;
            const auto source = input ? passIndices.find(input->name) : passIndices.end();
            if (source != passIndices.end())
            {
                channelInput.sourcePassIndex = source->second;
                channelInput.samplePreviousFrame = source->second >= i;
                if (input->filter == TextureFilter::Mipmap)
                {
                    m_passes[source->second].mipmaps = true;
                }
            }
            else if (input)
            {
                const rad::FilePath texturePath =
                    (GetAssetDirectory() / rad::MakeFilePath(input->name)).lexically_normal();
                const auto key = std::tuple{texturePath, input->vflip,
                                            input->filter == TextureFilter::Mipmap};
                auto found = textures.find(key);
                if (found == textures.end())
                {
                    found = textures.emplace(
                        key, LoadTexture(device, texturePath, input->vflip,
                                          input->filter == TextureFilter::Mipmap)).first;
                }
                channelInput.texture = found->second;
                samplingFeatures = device->GetPhysicalDevice()
                                       .getFormatProperties(found->second.image->GetFormat(),
                                                             device->GetDispatcher())
                                       .optimalTilingFeatures;
            }
            // Unbound channels use nearest sampling of the 1x1 black texture.
            const bool nearest = !input || input->filter == TextureFilter::Nearest;
            if (!nearest && !(samplingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear))
            {
                throw std::runtime_error("Linear sampling is unsupported in pass " +
                                         m_config.passes[i].name + "; use filter: nearest");
            }
            const bool mipmaps = input && input->filter == TextureFilter::Mipmap;
            const bool repeat = input && input->repeat;
            const auto samplerKey =
                std::pair{input ? input->filter : TextureFilter::Nearest, repeat};
            auto sampler = samplers.find(samplerKey);
            if (sampler == samplers.end())
            {
                sampler = samplers.emplace(
                    samplerKey, device->CreateSampler(
                        nearest ? vk::Filter::eNearest : vk::Filter::eLinear,
                        repeat ? vk::SamplerAddressMode::eRepeat : vk::SamplerAddressMode::eClampToEdge,
                        mipmaps ? vk::SamplerMipmapMode::eLinear : vk::SamplerMipmapMode::eNearest,
                        mipmaps ? VK_LOD_CLAMP_NONE : 0.0f)).first;
            }
            channelInput.sampler = sampler->second;
        }
    }
}

Renderer::RenderTarget Renderer::CreateRenderTarget(vk::Extent2D extent, bool mipmaps)
{
    const auto& limits = m_device->GetProperties().limits;
    if (extent.width == 0 || extent.height == 0 ||
        extent.width > limits.maxImageDimension2D || extent.height > limits.maxImageDimension2D ||
        extent.width > limits.maxFramebufferWidth || extent.height > limits.maxFramebufferHeight)
    {
        throw std::invalid_argument("Buffer extent exceeds Vulkan device limits");
    }
    const uint32_t levels = mipmaps ? GetMipLevelCount(extent) : 1;
    if (levels > 1)
    {
        CheckMipmapSupport(m_device.get(), BufferFormat);
    }
    auto usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled |
                 vk::ImageUsageFlagBits::eTransferDst;
    if (levels > 1)
    {
        usage |= vk::ImageUsageFlagBits::eTransferSrc;
    }
    RenderTarget target;
    target.image = m_device->CreateImage2D(
        extent.width, extent.height, BufferFormat, usage, VMA_MEMORY_USAGE_AUTO, 0, levels);
    target.attachmentView = target.image->CreateView2D();
    target.sampledView = levels == 1 ? target.attachmentView : target.image->CreateView2D(0, levels);
    return target;
}

void Renderer::Resize(vk::Extent2D extent, vk::Format screenFormat)
{
    const bool formatChanged = m_screenFormat != screenFormat;
    m_screenFormat = screenFormat;
    m_black = CreateRenderTarget({1, 1});
    for (std::size_t i = 0; i < m_passes.size(); ++i)
    {
        auto& runtime = m_passes[i];
        const PassConfig& pass = m_config.passes[i];
        const bool renderToTexture = i + 1 < m_passes.size();
        // Recreated views can reuse old Vulkan handle values; invalidate cached bindings.
        runtime.boundViews.assign(m_frameSlotCount, {});
        runtime.extent = pass.size[0] == 0 ? extent : vk::Extent2D{pass.size[0], pass.size[1]};
        if (renderToTexture)
        {
            for (auto& target : runtime.targets)
            {
                target = CreateRenderTarget(runtime.extent, runtime.mipmaps);
            }
        }
        if (!runtime.pipeline || (!renderToTexture && formatChanged))
        {
            CreatePipeline(i, renderToTexture ? BufferFormat : screenFormat);
        }
    }
    InitializeTargets();
}

void Renderer::InitializeTargets()
{
    auto pool = m_device->CreateCommandPool(sd::VulkanQueueFamily::Graphics);
    auto command = pool->AllocateCommandBuffer(vk::CommandBufferLevel::ePrimary);
    command->Begin();
    const auto clear = [&command](RenderTarget& target)
    {
        const vk::ImageSubresourceRange range{
            vk::ImageAspectFlagBits::eColor, 0, target.image->GetMipLevels(), 0, 1};
        command->ImageMemoryBarrier(
            target.image->GetHandle(), range, vk::ImageLayout::eUndefined,
            vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits2::eNone,
            vk::AccessFlagBits2::eNone, vk::PipelineStageFlagBits2::eClear,
            vk::AccessFlagBits2::eTransferWrite);
        command->ClearColorImage(target.image->GetHandle(), vk::ImageLayout::eTransferDstOptimal,
                                  vk::ClearColorValue(Float4{}), range);
        command->ImageMemoryBarrier(
            target.image->GetHandle(), range, vk::ImageLayout::eTransferDstOptimal,
            vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eClear,
            vk::AccessFlagBits2::eTransferWrite, vk::PipelineStageFlagBits2::eFragmentShader,
            vk::AccessFlagBits2::eShaderSampledRead);
    };
    clear(m_black);
    for (auto& pass : m_passes)
    {
        for (auto& target : pass.targets)
        {
            if (target.image)
            {
                clear(target);
            }
        }
    }
    command->End();
    auto* queue = m_device->GetQueue(sd::VulkanQueueFamily::Graphics);
    queue->Submit(command.get());
    queue->WaitIdle();
}

void Renderer::CreatePipeline(std::size_t passIndex, vk::Format format)
{
    sd::VulkanGraphicsPipelineCreateInfo info;
    info.m_stages.emplace_back(vk::ShaderStageFlagBits::eVertex, m_vertex);
    const VkBool32 renderToTexture = passIndex + 1 < m_passes.size() ? VK_TRUE : VK_FALSE;
    info.m_stages.back().SetSpecialization(0, renderToTexture);
    info.m_stages.emplace_back(vk::ShaderStageFlagBits::eFragment, m_passes[passIndex].shader);
    const int32_t colorAttachmentEncoding = IsSrgb(format) ? 1 : 0;
    info.m_stages.back().SetSpecialization(0, colorAttachmentEncoding);
    info.m_layout = m_layout;
    info.m_colorAttachmentFormats = {format};
    info.m_colorBlendState.attachments = {sd::MakeOpaqueColorBlendAttachmentState()};
    m_passes[passIndex].pipeline = m_device->CreateGraphicsPipeline(info);
}

void Renderer::UpdatePassResources(std::size_t passIndex, uint32_t frameSlot,
                                   const FrameData& frame)
{
    Pass& runtime = m_passes[passIndex];
    const float width = static_cast<float>(runtime.extent.width);
    const float height = static_cast<float>(runtime.extent.height);
    const uint32_t current = frame.index % 2;
    const uint32_t previous = 1 - current;
    Uniforms uniform{};
    uniform.resolutionTime = {width, height, 1.0f, frame.time};
    uniform.delta = frame.delta;
    uniform.frame = static_cast<int32_t>(frame.index);
    uniform.frameRate = frame.frameRate;
    uniform.sampleRate = 44100.0f;
    uniform.mouse = {frame.mouse[0] * width, frame.mouse[1] * height,
                     frame.mouse[2] * width, frame.mouse[3] * height};
    uniform.date = frame.date;
    std::array<vk::DescriptorImageInfo, 4> images;
    std::array<vk::WriteDescriptorSet, 4> writes;
    uint32_t writeCount = 0;
    for (uint32_t channel = 0; channel < runtime.inputs.size(); ++channel)
    {
        const InputChannel& input = runtime.inputs[channel];
        const sd::VulkanImage* image = m_black.image.get();
        const sd::VulkanImageView* view = m_black.sampledView.get();
        if (input.sourcePassIndex)
        {
            const RenderTarget& source = m_passes[*input.sourcePassIndex].targets[
                input.samplePreviousFrame ? previous : current];
            image = source.image.get();
            view = source.sampledView.get();
            uniform.channelTime[channel][0] = source.time;
        }
        else if (input.texture.image)
        {
            image = input.texture.image.get();
            view = input.texture.view.get();
        }
        if (input.sourcePassIndex || input.texture.image)
        {
            uniform.channelResolution[channel] = {
                static_cast<float>(image->GetWidth()),
                static_cast<float>(image->GetHeight()), 1.0f, 0.0f};
        }
        if (runtime.boundViews[frameSlot][channel] != view->GetHandle())
        {
            images[channel] = vk::DescriptorImageInfo{
                input.sampler->GetHandle(), view->GetHandle(),
                vk::ImageLayout::eShaderReadOnlyOptimal};
            writes[writeCount++] = vk::WriteDescriptorSet{
                runtime.descriptorSets[frameSlot]->GetHandle(), channel + 1, 0, 1,
                vk::DescriptorType::eCombinedImageSampler, &images[channel]};
            runtime.boundViews[frameSlot][channel] = view->GetHandle();
        }
    }
    if (writeCount != 0)
    {
        m_device->UpdateDescriptorSets(vk::ArrayProxy<const vk::WriteDescriptorSet>{
            writeCount, writes.data()});
    }
    const vk::DeviceSize offset = m_uniformStride * (passIndex * m_frameSlotCount + frameSlot);
    std::memcpy(m_uniformMapping + offset, &uniform, sizeof(uniform));
}

void Renderer::Render(sd::VulkanCommandBuffer* command, sd::VulkanImageView* screen,
                      uint32_t frameSlot, const FrameData& frame)
{
    if (frameSlot >= m_frameSlotCount ||
        frame.index > static_cast<uint32_t>(std::numeric_limits<int32_t>::max()))
    {
        throw std::invalid_argument("Invalid frame slot or exhausted Shadertoy frame counter");
    }
    const uint32_t current = frame.index % 2;
    for (std::size_t i = 0; i < m_passes.size(); ++i)
    {
        auto& runtime = m_passes[i];
        const bool renderToTexture = i + 1 < m_passes.size();
        UpdatePassResources(i, frameSlot, frame);
        const float width = static_cast<float>(runtime.extent.width);
        const float height = static_cast<float>(runtime.extent.height);
        RenderTarget& output = runtime.targets[current];
        if (renderToTexture)
        {
            // Covers reads in preceding passes/submissions before reusing a ping-pong target.
            command->ImageMemoryBarrier(
                output.image->GetHandle(), ColorRange, vk::ImageLayout::eShaderReadOnlyOptimal,
                vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eAllCommands,
                vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite,
                vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                vk::AccessFlagBits2::eColorAttachmentWrite);
        }
        sd::VulkanRenderingAttachmentInfo attachment;
        attachment.imageView = renderToTexture ? output.attachmentView.get() : screen;
        attachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
        attachment.loadOp = vk::AttachmentLoadOp::eClear;
        attachment.storeOp = vk::AttachmentStoreOp::eStore;
        const vk::Rect2D area{{0, 0}, runtime.extent};
        command->BeginRendering({attachment}, nullptr, nullptr, &area);
        command->BindPipeline(vk::PipelineBindPoint::eGraphics, runtime.pipeline->GetHandle());
        command->BindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_layout->GetHandle(),
                                     0, runtime.descriptorSets[frameSlot]->GetHandle());
        command->SetViewportWithCount(vk::Viewport{0.0f, 0.0f, width, height, 0.0f, 1.0f});
        command->SetScissorWithCount(area);
        command->Draw(3, 1, 0, 0);
        command->EndRendering();
        if (renderToTexture)
        {
            PrepareTextureForSampling(command, output.image.get(),
                                      vk::ImageLayout::eColorAttachmentOptimal,
                                      vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                      vk::AccessFlagBits2::eColorAttachmentWrite, true);
            output.time = frame.time;
        }
    }
}

} // namespace shadertoy
