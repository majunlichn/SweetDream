#pragma once

#include <SweetDream/Scene/Material.h>

#include <rad/Core/RefCounted.h>
#include <rad/Math/Vector.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace sd
{

class AxisAlignedBounds
{
public:
    rad::Vector3f m_min = rad::Vector3f::Zero();
    rad::Vector3f m_max = rad::Vector3f::Zero();
}; // class AxisAlignedBounds

enum class MeshTopology
{
    PointList,
    LineList,
    TriangleList,
    PolygonList
};

class MeshPrimitive
{
public:
    // Indices into the containing mesh's vertex streams. Surface faces use CCW winding.
    std::vector<std::uint32_t> m_indices;
    rad::Ref<Material> m_material;
    MeshTopology m_topology = MeshTopology::TriangleList;
    // PolygonList only: one count per face (at least 3), in index-buffer order.
    std::vector<std::uint32_t> m_faceVertexCounts = {};
}; // class MeshPrimitive

class Mesh final : public rad::RefCounted<Mesh>
{
public:
    [[nodiscard]] static rad::Ref<Mesh> Create() { return rad::Ref<Mesh>{new Mesh}; }

    std::string m_name;
    std::vector<rad::Vector3f> m_positions;
    // Optional streams are empty or contain one element per position.
    std::vector<rad::Vector3f> m_normals;
    std::vector<rad::Vector4f> m_tangents; // xyz direction, w bitangent sign (+1 or -1).
    std::vector<std::vector<rad::Vector2f>> m_texCoords;
    std::vector<rad::Vector4f> m_colors;
    std::vector<MeshPrimitive> m_primitives;

    // Mesh-local bounds; keep them consistent with m_positions.
    std::optional<AxisAlignedBounds> m_bounds;

    [[nodiscard]] std::optional<AxisAlignedBounds> ComputeBounds() const;
    // Call after editing positions. Empty or non-finite positions clear the cache.
    [[nodiscard]] bool UpdateBounds();
}; // class Mesh

} // namespace sd
