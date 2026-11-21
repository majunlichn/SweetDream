#pragma once

#include <rad/Core/FilePath.h>
#include <rad/Core/RefCounted.h>
#include <rad/Math/Vector.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace sd
{

enum class TextureSourceType
{
    ExternalFile,
    Encoded,
    Rgba8
};

class Texture final : public rad::RefCounted<Texture>
{
public:
    [[nodiscard]] static rad::Ref<Texture> Create() { return rad::Ref<Texture>{new Texture}; }

    std::string m_name;
    TextureSourceType m_sourceType = TextureSourceType::ExternalFile;
    rad::FilePath m_path; // ExternalFile only.
    std::vector<std::byte> m_bytes; // Encoded image or tightly packed RGBA8 rows, top to bottom.
    std::string m_mimeType; // Optional hint for Encoded.
    std::uint32_t m_width = 0; // Rgba8 only.
    std::uint32_t m_height = 0;
}; // class Texture

enum class TextureWrap
{
    Repeat,
    ClampToEdge,
    MirroredRepeat
};

enum class TextureFilter
{
    Nearest,
    Linear
};

enum class TextureMipFilter
{
    None,
    Nearest,
    Linear
};

class TextureBinding
{
public:
    rad::Ref<Texture> m_texture;
    std::size_t m_texCoord = 0;
    TextureWrap m_wrapU = TextureWrap::Repeat;
    TextureWrap m_wrapV = TextureWrap::Repeat;
    TextureFilter m_minFilter = TextureFilter::Linear;
    TextureFilter m_magFilter = TextureFilter::Linear;
    TextureMipFilter m_mipFilter = TextureMipFilter::Linear;
}; // class TextureBinding

enum class AlphaMode
{
    Opaque,
    Mask,
    Blend
};

class Material final : public rad::RefCounted<Material>
{
public:
    [[nodiscard]] static rad::Ref<Material> Create() { return rad::Ref<Material>{new Material}; }

    std::string m_name;
    rad::Vector4f m_baseColor = rad::Vector4f::Ones();
    rad::Vector3f m_emissive = rad::Vector3f::Zero();
    float m_metallic = 0.0f;
    float m_roughness = 1.0f;
    float m_normalScale = 1.0f;
    float m_occlusionStrength = 1.0f;
    AlphaMode m_alphaMode = AlphaMode::Opaque;
    float m_alphaCutoff = 0.5f;
    bool m_doubleSided = false;
    // Base color and emissive texture RGB are sRGB; other channels are linear.
    std::optional<TextureBinding> m_baseColorTexture;
    std::optional<TextureBinding> m_metallicRoughnessTexture; // G: roughness, B: metallic.
    std::optional<TextureBinding> m_normalTexture;
    std::optional<TextureBinding> m_occlusionTexture; // R: occlusion.
    std::optional<TextureBinding> m_emissiveTexture;
}; // class Material

} // namespace sd
