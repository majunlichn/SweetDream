#pragma once

#include <SweetDream/Scene/Camera.h>
#include <SweetDream/Scene/Light.h>
#include <SweetDream/Scene/Material.h>
#include <SweetDream/Scene/Mesh.h>

#include <rad/Core/RefCounted.h>
#include <rad/Core/Result.h>
#include <rad/Math/Matrix.h>

#include <boost/outcome/result.hpp>

#include <optional>
#include <span>
#include <string>
#include <vector>

namespace sd
{

enum class SceneErrorCode
{
    InvalidReference,
    InvalidTransform,
    HierarchyCycle,
    InvalidMesh,
    InvalidMaterial,
    InvalidTexture,
    InvalidCamera,
    InvalidLight
};

class SceneError
{
public:
    SceneErrorCode m_code;
    std::string m_message;
}; // class SceneError

template <typename T>
using SceneResult = BOOST_OUTCOME_V2_NAMESPACE::checked<T, SceneError>;

class Scene;

class SceneNode final : public rad::RefCounted<SceneNode>
{
public:
    ~SceneNode();
    SceneNode(const SceneNode&) = delete;
    SceneNode& operator=(const SceneNode&) = delete;

    [[nodiscard]] static rad::Ref<SceneNode> Create();

    // Borrowed; null for a root or when the parent has been destroyed.
    [[nodiscard]] const SceneNode* GetParent() const noexcept { return m_parent; }
    [[nodiscard]] std::span<const rad::Ref<SceneNode>> GetChildren() const noexcept
    {
        return m_children;
    }
    // Last successful scene update; local and hierarchy edits do not refresh it.
    [[nodiscard]] const std::optional<rad::Matrix4f>& GetCachedWorldTransform() const noexcept
    {
        return m_cachedWorldTransform;
    }

    // Direct edits are unchecked. Validate the scene before consuming edited data.
    std::string m_name;
    rad::Matrix4f m_localTransform = rad::Matrix4f::Identity();
    std::vector<rad::Ref<Mesh>> m_meshes;
    std::optional<Camera> m_camera;
    std::optional<Light> m_light;

private:
    friend class Scene;

    SceneNode() = default;

    SceneNode* m_parent = nullptr;
    const Scene* m_rootScene = nullptr; // Set only for roots attached to a scene.
    std::vector<rad::Ref<SceneNode>> m_children;
    std::optional<rad::Matrix4f> m_cachedWorldTransform;
}; // class SceneNode

// Right-handed, Y up, meters, column vectors. Cameras and oriented lights look along local -Z.
class Scene final : public rad::RefCounted<Scene>
{
public:
    enum class ReparentMode
    {
        KeepLocal,
        KeepWorld
    };

    Scene() = default;
    ~Scene();
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    [[nodiscard]] static rad::Ref<Scene> Create() { return rad::Ref<Scene>{new Scene}; }

    [[nodiscard]] SceneResult<rad::Ref<SceneNode>> AddNode(const rad::Ref<SceneNode>& parent = {});
    // Only a detached subtree root can be attached. Its local transform is unchanged.
    [[nodiscard]] SceneResult<void> AttachNode(rad::Ref<SceneNode> node,
                                               const rad::Ref<SceneNode>& parent = {});
    [[nodiscard]] SceneResult<void> DetachNode(rad::Ref<SceneNode> node);
    [[nodiscard]] SceneResult<void> SetLocalTransform(const rad::Ref<SceneNode>& node,
                                                      const rad::Matrix4f& localTransform);
    // KeepWorld fails without changing the hierarchy if the new parent is singular or
    // the current world pose cannot be preserved at scene precision.
    [[nodiscard]] SceneResult<void> ReparentNode(rad::Ref<SceneNode> node,
                                                const rad::Ref<SceneNode>& parent,
                                                ReparentMode mode = ReparentMode::KeepLocal);

    [[nodiscard]] std::span<const rad::Ref<SceneNode>> GetRoots() const noexcept { return m_roots; }
    [[nodiscard]] SceneResult<void> Validate() const;

    [[nodiscard]] SceneResult<rad::Matrix4f> GetWorldTransform(
        const rad::Ref<SceneNode>& node) const;
    // Updates all attached nodes. On failure, no cache entries are changed.
    [[nodiscard]] SceneResult<void> UpdateWorldTransforms();

private:
    [[nodiscard]] bool Contains(const SceneNode* node) const noexcept;
    [[nodiscard]] std::vector<rad::Ref<SceneNode>>& GetSiblings(SceneNode* parent) noexcept;

    std::vector<rad::Ref<SceneNode>> m_roots;
}; // class Scene

} // namespace sd
