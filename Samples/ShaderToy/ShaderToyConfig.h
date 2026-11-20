#pragma once

#include <rad/Core/FilePath.h>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace shadertoy
{

enum class TextureFilter
{
    Nearest,
    Linear,
    Mipmap,
};

struct InputChannelConfig
{
    std::string name;
    TextureFilter filter = TextureFilter::Linear;
    bool repeat = false;
    bool vflip = false;
};

struct PassConfig
{
    std::string name;
    rad::FilePath shaderPath;
    std::vector<std::optional<InputChannelConfig>> inputs;
    std::array<uint32_t, 2> size = {};
};

struct EffectConfig
{
    rad::FilePath common;
    std::vector<PassConfig> passes;
};

struct CommandLineOptions
{
    int width = 1280;
    int height = 720;
    uint32_t gpuIndex = 0;
    rad::FilePath config;
    uint32_t frames = 0;
    bool validate = false;
    bool help = false;
};

[[nodiscard]] CommandLineOptions ParseCommandLineOptions(std::span<const std::string> arguments);
[[nodiscard]] rad::FilePath GetAssetDirectory();
[[nodiscard]] EffectConfig ParseEffectConfig(const std::string& json, const rad::FilePath& directory);
[[nodiscard]] EffectConfig LoadEffectConfig(const rad::FilePath& path);

} // namespace shadertoy
