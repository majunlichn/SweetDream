#include <SweetDream/Scene/Scene.h>

#include <rad/Math/Transform3D.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <unordered_set>
#include <utility>

namespace sd
{
namespace
{

using Error = SceneErrorCode;

[[nodiscard]] auto Fail(Error code, std::string message)
{
    return rad::Failure(SceneError{code, std::move(message)});
}

[[nodiscard]] bool IsAffine(const rad::Matrix4f& matrix)
{
    return matrix.allFinite() && matrix.row(3) == rad::Vector4f::UnitW().transpose();
}

[[nodiscard]] bool IsNonnegative(float value)
{
    return std::isfinite(value) && value >= 0.0f;
}

[[nodiscard]] bool IsUnitInterval(float value)
{
    return value >= 0.0f && value <= 1.0f;
}

[[nodiscard]] bool IsValid(const Light& light)
{
    if (!light.m_color.allFinite() || (light.m_color.array() < 0.0f).any() ||
        !IsNonnegative(light.m_intensity))
    {
        return false;
    }
    switch (light.m_type)
    {
    case LightType::Directional:
        return true;
    case LightType::Point:
        return light.m_range > 0.0f;
    case LightType::Spot:
        return light.m_range > 0.0f && light.m_innerConeAngle >= 0.0f &&
               light.m_innerConeAngle < light.m_outerConeAngle &&
               light.m_outerConeAngle <= std::numbers::pi_v<float> / 2.0f;
    }
    return false;
}

template <typename T>
[[nodiscard]] bool IsValidStream(const std::vector<T>& stream, std::size_t vertexCount)
{
    return stream.empty() || (stream.size() == vertexCount &&
        std::ranges::all_of(stream, [](const T& value) { return value.allFinite(); }));
}

[[nodiscard]] const char* GetTopologyError(const MeshPrimitive& primitive)
{
    const auto indexCount = primitive.m_indices.size();
    if (indexCount == 0)
    {
        return "Primitive requires at least one index";
    }
    if (primitive.m_topology != MeshTopology::PolygonList && !primitive.m_faceVertexCounts.empty())
    {
        return "Only PolygonList accepts per-face vertex counts";
    }

    switch (primitive.m_topology)
    {
    case MeshTopology::PointList:
        return nullptr;
    case MeshTopology::LineList:
        return indexCount % 2 == 0 ? nullptr : "LineList requires pairs of indices";
    case MeshTopology::TriangleList:
        return indexCount % 3 == 0 ? nullptr : "TriangleList requires triples of indices";
    case MeshTopology::PolygonList:
    {
        auto remaining = indexCount;
        for (const auto faceSize : primitive.m_faceVertexCounts)
        {
            if (faceSize < 3 || faceSize > remaining)
            {
                return "PolygonList face must have at least three vertices and fit the index buffer";
            }
            remaining -= faceSize;
        }
        return remaining == 0 ? nullptr : "PolygonList face sizes must account for every index";
    }
    }
    return "Unknown mesh topology";
}

[[nodiscard]] SceneResult<void> ValidateTexture(const Texture& texture)
{
    switch (texture.m_sourceType)
    {
    case TextureSourceType::ExternalFile:
        if (texture.m_path.empty())
        {
            return Fail(Error::InvalidTexture, "External texture requires a file path");
        }
        return rad::Success();
    case TextureSourceType::Encoded:
        if (texture.m_bytes.empty())
        {
            return Fail(Error::InvalidTexture, "Encoded texture requires image bytes");
        }
        return rad::Success();
    case TextureSourceType::Rgba8:
    {
        const auto byteCount = texture.m_bytes.size();
        const auto pixelCount = byteCount / 4;
        if (texture.m_width == 0 || texture.m_height == 0 || byteCount % 4 != 0 ||
            pixelCount % texture.m_width != 0 || pixelCount / texture.m_width != texture.m_height)
        {
            return Fail(Error::InvalidTexture, "RGBA8 texture byte count must match its dimensions");
        }
        return rad::Success();
    }
    }
    return Fail(Error::InvalidTexture, "Unknown texture source type");
}

[[nodiscard]] SceneResult<void> ValidateMaterial(const Material& material, const Mesh& mesh)
{
    if (!(material.m_baseColor.array() >= 0.0f).all() ||
        !(material.m_baseColor.array() <= 1.0f).all() ||
        !material.m_emissive.allFinite() || (material.m_emissive.array() < 0.0f).any() ||
        !IsUnitInterval(material.m_metallic) || !IsUnitInterval(material.m_roughness) ||
        !std::isfinite(material.m_normalScale) || !IsUnitInterval(material.m_occlusionStrength) ||
        !IsUnitInterval(material.m_alphaCutoff))
    {
        return Fail(Error::InvalidMaterial, "PBR color or scalar factor is out of range");
    }

    const std::array bindings{
        std::pair{"baseColorTexture", &material.m_baseColorTexture},
        std::pair{"metallicRoughnessTexture", &material.m_metallicRoughnessTexture},
        std::pair{"normalTexture", &material.m_normalTexture},
        std::pair{"occlusionTexture", &material.m_occlusionTexture},
        std::pair{"emissiveTexture", &material.m_emissiveTexture},
    };
    for (const auto& [slot, binding] : bindings)
    {
        if (!*binding)
        {
            continue;
        }
        if (!(*binding)->m_texture)
        {
            return Fail(Error::InvalidReference, "Material has a null texture binding");
        }
        if (auto result = ValidateTexture(*(*binding)->m_texture); !result)
        {
            return result;
        }
        const auto uv = (*binding)->m_texCoord;
        if (uv >= mesh.m_texCoords.size() || mesh.m_texCoords[uv].empty())
        {
            return Fail(Error::InvalidMesh, std::string(slot) + " requires missing UV set " +
                            std::to_string(uv));
        }
    }

    return rad::Success();
}

[[nodiscard]] SceneResult<void> ValidateMesh(const Mesh& mesh)
{
    const auto count = mesh.m_positions.size();
    if (count == 0 || mesh.m_primitives.empty() || !IsValidStream(mesh.m_positions, count))
    {
        return Fail(Error::InvalidMesh, "Mesh requires finite positions and at least one primitive");
    }
    if (!IsValidStream(mesh.m_normals, count) || !IsValidStream(mesh.m_tangents, count) ||
        !IsValidStream(mesh.m_colors, count) ||
        !std::ranges::all_of(mesh.m_texCoords, [count](const auto& uv)
        {
            return IsValidStream(uv, count);
        }))
    {
        return Fail(Error::InvalidMesh, "Vertex stream size or values do not match the mesh");
    }
    if (!std::ranges::all_of(mesh.m_tangents,
        [](const auto& tangent) { return tangent.w() == 1.0f || tangent.w() == -1.0f; }))
    {
        return Fail(Error::InvalidMesh, "Tangent bitangent signs must be +1 or -1");
    }
    for (std::size_t i = 0; i < mesh.m_primitives.size(); ++i)
    {
        const auto& primitive = mesh.m_primitives[i];
        if (const auto* error = GetTopologyError(primitive))
        {
            return Fail(Error::InvalidMesh, error);
        }
        if (std::ranges::any_of(primitive.m_indices, [count](auto index) { return index >= count; }))
        {
            return Fail(Error::InvalidMesh, "Primitive index is outside the mesh's vertex streams");
        }
        if (primitive.m_material)
        {
            if (auto result = ValidateMaterial(*primitive.m_material, mesh); !result)
            {
                return Fail(result.error().m_code, "Primitive " + std::to_string(i) + ": " +
                                result.error().m_message);
            }
        }
    }

    return rad::Success();
}

[[nodiscard]] SceneResult<void> ValidateNode(const SceneNode& node,
                                            std::unordered_set<const Mesh*>& checkedMeshes)
{
    if (!IsAffine(node.m_localTransform))
    {
        return Fail(Error::InvalidTransform, "Node transform must be finite and affine");
    }
    if (node.m_camera)
    {
        const auto projection = node.m_camera->ComputeProjection(rad::ClipDepth::ZeroToOne);
        if (!projection)
        {
            return Fail(Error::InvalidCamera, projection.error() == std::errc::result_out_of_range
                            ? "Camera projection is not representable"
                            : "Invalid camera type or projection parameters");
        }
    }
    if (node.m_light && !IsValid(*node.m_light))
    {
        return Fail(Error::InvalidLight, "Invalid light type, color, intensity, range, or cone");
    }
    for (std::size_t i = 0; i < node.m_meshes.size(); ++i)
    {
        const auto& mesh = node.m_meshes[i];
        if (!mesh)
        {
            return Fail(Error::InvalidReference, "Node has a null mesh reference");
        }
        if (checkedMeshes.insert(mesh.get()).second)
        {
            if (auto result = ValidateMesh(*mesh); !result)
            {
                const auto& nodeName = node.m_name;
                const auto& meshName = mesh->m_name;
                return Fail(result.error().m_code,
                            "Node '" + (nodeName.empty() ? "<unnamed>" : nodeName) +
                                "', mesh " + std::to_string(i) + " '" +
                                (meshName.empty() ? "<unnamed>" : meshName) + "': " +
                                result.error().m_message);
            }
        }
    }
    return rad::Success();
}

[[nodiscard]] SceneResult<void> ValidateSubtrees(std::span<const rad::Ref<SceneNode>> roots)
{
    std::unordered_set<const Mesh*> checkedMeshes;
    std::vector<const SceneNode*> pending;
    for (const auto& root : roots)
    {
        pending.push_back(root.get());
    }
    while (!pending.empty())
    {
        const auto* node = pending.back();
        pending.pop_back();
        if (auto result = ValidateNode(*node, checkedMeshes); !result)
        {
            return result;
        }
        for (const auto& child : node->GetChildren())
        {
            pending.push_back(child.get());
        }
    }
    return rad::Success();
}

[[nodiscard]] SceneResult<rad::Matrix4f> ComputeWorldTransform(const SceneNode* node)
{
    std::vector<const SceneNode*> chain;
    for (auto* ancestor = node; ancestor; ancestor = ancestor->GetParent())
    {
        chain.push_back(ancestor);
    }
    rad::Matrix4f world = rad::Matrix4f::Identity();
    for (auto it = chain.rbegin(); it != chain.rend(); ++it)
    {
        const auto& local = (*it)->m_localTransform;
        if (!IsAffine(local))
        {
            return Fail(Error::InvalidTransform, "Node transform must be finite and affine");
        }
        world *= local;
    }
    if (!world.allFinite())
    {
        return Fail(Error::InvalidTransform, "World transform is not representable");
    }
    return rad::Success(std::move(world));
}

} // namespace

rad::Ref<SceneNode> SceneNode::Create()
{
    return rad::Ref<SceneNode>{new SceneNode};
}

SceneNode::~SceneNode()
{
    // Walk uniquely owned descendants before releasing them, so deep trees do not
    // consume one destructor stack frame per node. Retained subtrees are left intact.
    auto* node = this;
    for (;;)
    {
        if (node->m_children.empty())
        {
            if (node == this)
            {
                break;
            }
            auto* parent = node->m_parent;
            node->m_parent = nullptr;
            parent->m_children.pop_back();
            node = parent;
            continue;
        }

        const auto& child = node->m_children.back();
        if (child->use_count() == 1)
        {
            node = child.get();
        }
        else
        {
            child->m_parent = nullptr;
            node->m_children.pop_back();
        }
    }
}

Scene::~Scene()
{
    for (const auto& root : m_roots)
    {
        root->m_rootScene = nullptr;
    }
}

SceneResult<rad::Ref<SceneNode>> Scene::AddNode(const rad::Ref<SceneNode>& parent)
{
    auto node = SceneNode::Create();
    if (auto result = AttachNode(node, parent); !result)
    {
        return rad::Failure(std::move(result).error());
    }
    return rad::Success(std::move(node));
}

SceneResult<void> Scene::AttachNode(rad::Ref<SceneNode> node, const rad::Ref<SceneNode>& parent)
{
    if (!node || node->m_parent || node->m_rootScene || (parent && !Contains(parent.get())))
    {
        return Fail(Error::InvalidReference, "Node must be detached and parent must belong to this scene");
    }
    if (auto result = ValidateSubtrees({&node, 1}); !result)
    {
        return result;
    }
    auto* newParent = parent.get();
    GetSiblings(newParent).push_back(node);
    node->m_parent = newParent;
    node->m_rootScene = newParent ? nullptr : this;
    return rad::Success();
}

SceneResult<void> Scene::DetachNode(rad::Ref<SceneNode> node)
{
    if (!Contains(node.get()))
    {
        return Fail(Error::InvalidReference, "Node does not belong to this scene");
    }
    std::erase(GetSiblings(node->m_parent), node);
    node->m_parent = nullptr;
    node->m_rootScene = nullptr;
    return rad::Success();
}

SceneResult<void> Scene::SetLocalTransform(const rad::Ref<SceneNode>& node,
                                          const rad::Matrix4f& localTransform)
{
    if (!Contains(node.get()))
    {
        return Fail(Error::InvalidReference, "Node does not belong to this scene");
    }
    if (!IsAffine(localTransform))
    {
        return Fail(Error::InvalidTransform, "Node transform must be finite and affine");
    }
    node->m_localTransform = localTransform;
    return rad::Success();
}

SceneResult<void> Scene::ReparentNode(rad::Ref<SceneNode> node,
                                     const rad::Ref<SceneNode>& parent, ReparentMode mode)
{
    auto* newParent = parent.get();
    if (!Contains(node.get()) || (newParent && !Contains(newParent)))
    {
        return Fail(Error::InvalidReference, "Node and new parent must belong to this scene");
    }
    if (node->m_parent == newParent)
    {
        return rad::Success();
    }
    for (auto* ancestor = newParent; ancestor; ancestor = ancestor->m_parent)
    {
        if (ancestor == node.get())
        {
            return Fail(Error::HierarchyCycle, "Cannot parent a node to itself or its descendants");
        }
    }

    rad::Matrix4f local = node->m_localTransform;
    if (mode == ReparentMode::KeepWorld)
    {
        auto world = ComputeWorldTransform(node.get());
        if (!world)
        {
            return rad::Failure(std::move(world).error());
        }
        local = world.value();
        if (newParent)
        {
            auto parentWorld = ComputeWorldTransform(newParent);
            if (!parentWorld)
            {
                return rad::Failure(std::move(parentWorld).error());
            }
            const rad::Matrix4d basis = parentWorld.value().cast<double>();
            const auto normal = rad::TryNormalMatrix(basis);
            if (!normal)
            {
                return Fail(Error::InvalidTransform, "New parent's world transform cannot be inverted");
            }
            // Invert only the linear part, keeping the affine row exact.
            const auto inverseLinear = normal.value().transpose();
            const rad::Matrix4d target = local.cast<double>();
            rad::Matrix4d relative = rad::Matrix4d::Identity();
            relative.topLeftCorner<3, 3>() = inverseLinear * target.topLeftCorner<3, 3>();
            relative.topRightCorner<3, 1>() = inverseLinear *
                (target.topRightCorner<3, 1>() - basis.topRightCorner<3, 1>());
            if (!relative.allFinite() ||
                relative.cwiseAbs().maxCoeff() > std::numeric_limits<float>::max())
            {
                return Fail(Error::InvalidTransform, "Reparented local transform is not representable");
            }
            local = relative.cast<float>();
            // A finite local matrix can still lose the requested pose when rounded,
            // especially after subtracting a large parent translation.
            const rad::Matrix4f recomposed = parentWorld.value() * local;
            constexpr double tolerance = 32.0 * std::numeric_limits<float>::epsilon();
            if (!recomposed.allFinite() ||
                ((recomposed.cast<double>() - target).cwiseAbs().array() >
                 tolerance * target.cwiseAbs().array().max(1.0)).any())
            {
                return Fail(Error::InvalidTransform, "World transform cannot be preserved at scene precision");
            }
        }
    }

    GetSiblings(newParent).push_back(node);
    std::erase(GetSiblings(node->m_parent), node);
    node->m_parent = newParent;
    node->m_rootScene = newParent ? nullptr : this;
    node->m_localTransform = local;
    return rad::Success();
}

SceneResult<void> Scene::Validate() const
{
    return ValidateSubtrees(m_roots);
}

SceneResult<rad::Matrix4f> Scene::GetWorldTransform(const rad::Ref<SceneNode>& node) const
{
    if (!Contains(node.get()))
    {
        return Fail(Error::InvalidReference, "Node does not belong to this scene");
    }
    return ComputeWorldTransform(node.get());
}

SceneResult<void> Scene::UpdateWorldTransforms()
{
    struct PendingNode
    {
        SceneNode* m_node;
        std::size_t m_parentIndex;
    };
    struct StagedTransform
    {
        SceneNode* m_node;
        rad::Matrix4f m_worldTransform;
    };
    constexpr auto noParent = std::numeric_limits<std::size_t>::max();
    std::vector<StagedTransform> worlds;
    std::vector<PendingNode> pending;
    for (auto it = m_roots.rbegin(); it != m_roots.rend(); ++it)
    {
        pending.push_back({it->get(), noParent});
    }
    while (!pending.empty())
    {
        const auto [node, parentIndex] = pending.back();
        pending.pop_back();
        const auto& local = node->m_localTransform;
        if (!IsAffine(local))
        {
            return Fail(Error::InvalidTransform, "Node transform must be finite and affine");
        }
        rad::Matrix4f world = local;
        if (parentIndex != noParent)
        {
            world = worlds[parentIndex].m_worldTransform * world;
        }
        if (!world.allFinite())
        {
            return Fail(Error::InvalidTransform, "World transform is not representable");
        }
        const auto index = worlds.size();
        worlds.push_back({node, std::move(world)});
        const auto& children = node->m_children;
        for (auto it = children.rbegin(); it != children.rend(); ++it)
        {
            pending.push_back({it->get(), index});
        }
    }
    for (auto& [node, world] : worlds)
    {
        node->m_cachedWorldTransform = std::move(world);
    }
    return rad::Success();
}

bool Scene::Contains(const SceneNode* node) const noexcept
{
    while (node && node->m_parent)
    {
        node = node->m_parent;
    }
    return node && node->m_rootScene == this;
}

std::vector<rad::Ref<SceneNode>>& Scene::GetSiblings(SceneNode* parent) noexcept
{
    return parent ? parent->m_children : m_roots;
}

} // namespace sd
