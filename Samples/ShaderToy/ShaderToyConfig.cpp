#include "ShaderToyConfig.h"

#include <rad/IO/File.h>
#include <rad/IO/Json.h>
#include <rad/System/OS.h>

#include <charconv>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace shadertoy
{
namespace
{

void CheckKeys(const rad::JsonObject& object,
               std::initializer_list<std::string_view> allowed)
{
    for (const auto& member : object)
    {
        bool known = false;
        for (const std::string_view key : allowed)
        {
            known |= member.key() == key;
        }
        if (!known)
        {
            throw std::invalid_argument("Unknown configuration field: " +
                                        std::string(member.key()));
        }
    }
}

std::string GetString(const rad::JsonObject& object, std::string_view key)
{
    const auto* value = object.if_contains(key);
    if (value == nullptr || !value->is_string() || value->as_string().empty())
    {
        throw std::invalid_argument("Expected a nonempty string for " + std::string(key));
    }
    return std::string(value->as_string());
}

bool GetChoice(const rad::JsonObject& object, std::string_view key,
               std::string_view defaultValue, std::string_view alternative)
{
    if (!object.contains(key))
    {
        return false;
    }
    const std::string value = GetString(object, key);
    if (value != defaultValue && value != alternative)
    {
        throw std::invalid_argument("Unsupported " + std::string(key) + ": " + value);
    }
    return value == alternative;
}

TextureFilter GetTextureFilter(const rad::JsonObject& object)
{
    if (!object.contains("filter"))
    {
        return TextureFilter::Linear;
    }
    const std::string value = GetString(object, "filter");
    if (value == "nearest")
    {
        return TextureFilter::Nearest;
    }
    if (value == "linear")
    {
        return TextureFilter::Linear;
    }
    if (value == "mipmap")
    {
        return TextureFilter::Mipmap;
    }
    throw std::invalid_argument("Unsupported filter: " + value);
}

bool GetBool(const rad::JsonObject& object, std::string_view key)
{
    const auto* value = object.if_contains(key);
    if (value == nullptr)
    {
        return false;
    }
    if (!value->is_bool())
    {
        throw std::invalid_argument(std::string(key) + " must be a boolean");
    }
    return value->as_bool();
}

InputChannelConfig ParseInputChannel(
    const rad::JsonObject& object,
    const std::unordered_map<std::string, std::size_t>& passIndices)
{
    CheckKeys(object, {"name", "filter", "wrap", "vflip"});
    InputChannelConfig input;
    input.name = GetString(object, "name");
    const bool bufferInput = passIndices.contains(input.name);
    if (bufferInput && object.contains("vflip"))
    {
        throw std::invalid_argument("vflip is only supported for file textures");
    }
    input.vflip = GetBool(object, "vflip");
    input.filter = GetTextureFilter(object);
    input.repeat = GetChoice(object, "wrap", "clamp", "repeat");
    return input;
}

uint32_t GetInteger(const rad::JsonValue& value, uint32_t maximum)
{
    if ((!value.is_int64() && !value.is_uint64()) ||
        (value.is_int64() && value.as_int64() < 0))
    {
        throw std::invalid_argument("Expected an integer in [0, " +
                                    std::to_string(maximum) + "]");
    }
    const uint64_t integer = value.is_uint64() ? value.as_uint64()
                                               : static_cast<uint64_t>(value.as_int64());
    if (integer > maximum)
    {
        throw std::invalid_argument("Expected an integer in [0, " +
                                    std::to_string(maximum) + "]");
    }
    return static_cast<uint32_t>(integer);
}

uint32_t ParseInteger(std::string_view text, uint32_t minimum, uint32_t maximum)
{
    uint32_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
        value < minimum || value > maximum)
    {
        throw std::invalid_argument("Expected an integer in [" + std::to_string(minimum) +
                                    ", " + std::to_string(maximum) + "]: " + std::string(text));
    }
    return value;
}

std::string ReadText(const rad::FilePath& path)
{
    auto text = rad::File::ReadAllText(path);
    if (!text)
    {
        throw std::runtime_error("Cannot read file: " + rad::ToUtf8(path));
    }
    return std::move(text.assume_value());
}

} // namespace

CommandLineOptions ParseCommandLineOptions(std::span<const std::string> arguments)
{
    CommandLineOptions options;
    options.config = GetAssetDirectory() / "Shaders" / "LightCycle" / "LightCycle.json";
    for (std::size_t i = 1; i < arguments.size(); ++i)
    {
        const std::string& argument = arguments[i];
        if (argument == "--help")
        {
            options.help = true;
        }
        else if (argument == "--validate")
        {
            options.validate = true;
        }
        else if (argument == "--width" || argument == "--height" ||
                 argument == "--config" || argument == "--frames" || argument == "--gpu-index")
        {
            if (++i == arguments.size())
            {
                throw std::invalid_argument("Missing value for " + argument);
            }
            if (argument == "--config")
            {
                options.config = rad::MakeFilePath(arguments[i]);
            }
            else
            {
                const uint32_t maximum = argument == "--gpu-index"
                                             ? std::numeric_limits<uint32_t>::max()
                                             : argument == "--frames"
                                                   ? static_cast<uint32_t>(std::numeric_limits<int32_t>::max())
                                                   : static_cast<uint32_t>(std::numeric_limits<int>::max());
                const uint32_t value = ParseInteger(
                    arguments[i], argument == "--gpu-index" ? 0 : 1, maximum);
                if (argument == "--width")
                {
                    options.width = static_cast<int>(value);
                }
                else if (argument == "--height")
                {
                    options.height = static_cast<int>(value);
                }
                else if (argument == "--frames")
                {
                    options.frames = value;
                }
                else
                {
                    options.gpuIndex = value;
                }
            }
        }
        else
        {
            throw std::invalid_argument("Unknown option: " + argument);
        }
    }
    return options;
}

rad::FilePath GetAssetDirectory()
{
    return rad::os::executable_directory() / "ShaderToy";
}

EffectConfig ParseEffectConfig(const std::string& json, const rad::FilePath& directory)
{
    auto parsed = rad::ParseJson(json);
    if (!parsed)
    {
        throw std::invalid_argument("Invalid JSON: " + parsed.assume_error().message());
    }
    const rad::JsonValue& root = parsed.assume_value();
    if (!root.is_object())
    {
        throw std::invalid_argument("Configuration must be an object");
    }
    const auto& object = root.as_object();
    CheckKeys(object, {"common", "passes"});
    EffectConfig config;
    if (object.contains("common"))
    {
        config.common = directory / rad::MakeFilePath(GetString(object, "common"));
    }
    const auto* passes = object.if_contains("passes");
    if (passes == nullptr || !passes->is_array() || passes->as_array().empty() ||
        passes->as_array().size() > 5)
    {
        throw std::invalid_argument("Expected 1 Image and up to 4 Buffer passes");
    }
    const auto& passArray = passes->as_array();
    std::unordered_map<std::string, std::size_t> passIndices;
    for (std::size_t i = 0; i < passArray.size(); ++i)
    {
        const auto& value = passArray[i];
        if (!value.is_object())
        {
            throw std::invalid_argument("Each pass must be an object");
        }
        const std::string name = GetString(value.as_object(), "name");
        if (!passIndices.emplace(name, i).second)
        {
            throw std::invalid_argument("Duplicate pass name: " + name);
        }
    }
    for (std::size_t i = 0; i < passArray.size(); ++i)
    {
        const rad::JsonValue& value = passArray[i];
        const auto& passObject = value.as_object();
        CheckKeys(passObject, {"name", "shader", "inputs", "size"});
        PassConfig pass;
        pass.name = GetString(passObject, "name");
        pass.shaderPath = directory / rad::MakeFilePath(GetString(passObject, "shader"));
        if (const auto* size = passObject.if_contains("size"))
        {
            if (i + 1 == passArray.size() || !size->is_array() || size->as_array().size() != 2)
            {
                throw std::invalid_argument("size must be [width, height] on a Buffer pass");
            }
            for (std::size_t i = 0; i < 2; ++i)
            {
                pass.size[i] = GetInteger(size->as_array()[i],
                                          static_cast<uint32_t>(std::numeric_limits<int>::max()));
                if (pass.size[i] == 0)
                {
                    throw std::invalid_argument("Buffer dimensions must be positive");
                }
            }
        }
        if (const auto* inputs = passObject.if_contains("inputs"))
        {
            if (!inputs->is_array())
            {
                throw std::invalid_argument("inputs must be an array");
            }
            if (inputs->as_array().size() > 4)
            {
                throw std::invalid_argument("inputs must contain at most 4 channels");
            }
            pass.inputs.reserve(inputs->as_array().size());
            for (const auto& inputValue : inputs->as_array())
            {
                if (inputValue.is_null())
                {
                    pass.inputs.emplace_back(std::nullopt);
                    continue;
                }
                if (!inputValue.is_object())
                {
                    throw std::invalid_argument("Each input must be an object or null");
                }
                pass.inputs.emplace_back(ParseInputChannel(inputValue.as_object(), passIndices));
            }
        }
        config.passes.push_back(std::move(pass));
    }
    const std::size_t imagePassIndex = config.passes.size() - 1;
    for (const PassConfig& pass : config.passes)
    {
        for (const auto& input : pass.inputs)
        {
            if (!input)
            {
                continue;
            }
            const auto source = passIndices.find(input->name);
            if (source == passIndices.end())
            {
                continue;
            }
            if (source->second == imagePassIndex)
            {
                throw std::invalid_argument("Input cannot sample the screen pass: " + input->name);
            }
        }
    }
    return config;
}

EffectConfig LoadEffectConfig(const rad::FilePath& path)
{
    const rad::FilePath absolute = std::filesystem::absolute(path);
    return ParseEffectConfig(ReadText(absolute), absolute.parent_path());
}

} // namespace shadertoy
