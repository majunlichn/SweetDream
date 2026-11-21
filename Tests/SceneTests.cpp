#include <SweetDream/Scene/Scene.h>

#include <rad/Math/Transform3D.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace
{

[[nodiscard]] rad::Ref<sd::Mesh> MakeTriangle()
{
    auto mesh = sd::Mesh::Create();
    mesh->m_positions = {{-1.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}, {0.0f, 3.0f, 0.0f}};
    mesh->m_primitives.push_back({{0, 1, 2}, {}});
    return mesh;
}

TEST(Scene, InstancesShareMeshAndMaterialEdits)
{
    sd::Scene scene;
    const auto texture = sd::Texture::Create();
    texture->m_name = "checker";
    texture->m_path = "checker.png";
    const auto opaque = sd::Material::Create();
    opaque->m_baseColorTexture = sd::TextureBinding{texture};
    const auto transparent = sd::Material::Create();
    transparent->m_baseColorTexture = sd::TextureBinding{texture};
    transparent->m_alphaMode = sd::AlphaMode::Blend;

    auto mesh = MakeTriangle();
    mesh->m_positions.emplace_back(0.0f, 0.0f, 4.0f);
    mesh->m_texCoords = {{{0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}}};
    mesh->m_primitives.front().m_material = opaque;
    mesh->m_primitives.push_back({{0, 2, 3}, transparent});

    const auto left = sd::SceneNode::Create();
    left->m_meshes.push_back(mesh);
    const auto right = sd::SceneNode::Create();
    right->m_meshes.push_back(mesh);
    ASSERT_TRUE(scene.AttachNode(left));
    ASSERT_TRUE(scene.AttachNode(right));
    const auto& shared = left->m_meshes.front();
    ASSERT_EQ(shared, mesh);
    ASSERT_EQ(shared, right->m_meshes.front());
    ASSERT_EQ(shared->m_primitives.size(), 2u);
    ASSERT_EQ(shared->m_primitives[0].m_material, opaque);
    ASSERT_EQ(shared->m_primitives[1].m_material, transparent);
    ASSERT_TRUE(opaque->m_baseColorTexture);
    EXPECT_EQ(shared->m_primitives[0].m_material->m_baseColorTexture->m_texture, texture);
    transparent->m_roughness = 0.25f;
    EXPECT_FLOAT_EQ(right->m_meshes.front()->m_primitives[1].m_material->m_roughness, 0.25f);
}

TEST(Mesh, RefreshesOrClearsBoundsAfterGeometryEdits)
{
    const auto mesh = MakeTriangle();
    ASSERT_TRUE(mesh->UpdateBounds());
    mesh->m_positions.emplace_back(4.0f, -2.0f, 5.0f);
    const auto fresh = mesh->ComputeBounds();
    ASSERT_TRUE(fresh);
    EXPECT_TRUE(fresh->m_max.isApprox(rad::Vector3f{4.0f, 3.0f, 5.0f}));
    ASSERT_TRUE(mesh->m_bounds);
    EXPECT_TRUE(mesh->m_bounds->m_max.isApprox(rad::Vector3f{2.0f, 3.0f, 0.0f}));
    ASSERT_TRUE(mesh->UpdateBounds());
    ASSERT_TRUE(mesh->m_bounds);
    EXPECT_TRUE(mesh->m_bounds->m_min.isApprox(rad::Vector3f{-1.0f, -2.0f, 0.0f}));
    EXPECT_TRUE(mesh->m_bounds->m_max.isApprox(fresh->m_max));

    mesh->m_positions.back().z() = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(mesh->UpdateBounds());
    EXPECT_FALSE(mesh->m_bounds);

    mesh->m_positions = {rad::Vector3f::Zero()};
    ASSERT_TRUE(mesh->UpdateBounds());
    const auto point = mesh->m_bounds;
    ASSERT_TRUE(point);
    EXPECT_TRUE(point->m_min.isZero());
    EXPECT_TRUE(point->m_max.isZero());

    mesh->m_positions.clear();
    EXPECT_FALSE(mesh->UpdateBounds());
    EXPECT_FALSE(mesh->m_bounds);
}

TEST(Scene, PreservesMixedTopologyAndRejectsMalformedPolygonEdits)
{
    sd::Scene scene;
    auto mesh = MakeTriangle();
    mesh->m_positions.emplace_back(-1.0f, 2.0f, 0.0f);
    mesh->m_positions.emplace_back(2.0f, 2.0f, 0.0f);
    mesh->m_primitives.push_back({{0}, {}, sd::MeshTopology::PointList});
    mesh->m_primitives.push_back({{0, 1}, {}, sd::MeshTopology::LineList});
    const std::vector<std::uint32_t> indices{0, 1, 2, 0, 1, 4, 3, 0, 1, 4, 2, 3};
    const std::vector<std::uint32_t> faceVertexCounts{3, 4, 5};
    mesh->m_primitives.push_back({indices, {}, sd::MeshTopology::PolygonList, faceVertexCounts});
    const auto node = sd::SceneNode::Create();
    node->m_meshes.push_back(mesh);
    ASSERT_TRUE(scene.AttachNode(node));
    const auto& stored = node->m_meshes.front();
    ASSERT_EQ(stored->m_primitives.size(), 4u);
    EXPECT_EQ(stored->m_primitives[0].m_topology, sd::MeshTopology::TriangleList);
    EXPECT_EQ(stored->m_primitives[1].m_topology, sd::MeshTopology::PointList);
    EXPECT_EQ(stored->m_primitives[2].m_topology, sd::MeshTopology::LineList);
    EXPECT_EQ(stored->m_primitives[3].m_topology, sd::MeshTopology::PolygonList);
    EXPECT_EQ(stored->m_primitives[3].m_indices, indices);
    EXPECT_EQ(stored->m_primitives[3].m_faceVertexCounts, faceVertexCounts);

    auto& polygons = mesh->m_primitives.back();
    polygons.m_faceVertexCounts.back() = 4;
    const auto incompleteFace = scene.Validate();
    ASSERT_FALSE(incompleteFace);
    EXPECT_EQ(incompleteFace.error().m_code, sd::SceneErrorCode::InvalidMesh);
    polygons.m_faceVertexCounts = faceVertexCounts;
    polygons.m_indices.back() = 5;
    const auto badIndex = scene.Validate();
    ASSERT_FALSE(badIndex);
    EXPECT_EQ(badIndex.error().m_code, sd::SceneErrorCode::InvalidMesh);
}

TEST(Scene, ValidatesTextureEditsWhenSwitchingFromFileToEmbeddedData)
{
    sd::Scene scene;
    const auto texture = sd::Texture::Create();
    texture->m_path = "checker.png";
    const auto material = sd::Material::Create();
    material->m_baseColorTexture = sd::TextureBinding{texture};
    const auto mesh = MakeTriangle();
    mesh->m_texCoords = {{{0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}}};
    mesh->m_primitives.front().m_material = material;
    const auto node = sd::SceneNode::Create();
    node->m_meshes.push_back(mesh);
    ASSERT_TRUE(scene.AttachNode(node));

    texture->m_sourceType = sd::TextureSourceType::Encoded;
    const auto noBytes = scene.Validate();
    ASSERT_FALSE(noBytes);
    EXPECT_EQ(noBytes.error().m_code, sd::SceneErrorCode::InvalidTexture);
    texture->m_bytes.resize(8);
    EXPECT_TRUE(scene.Validate());

    texture->m_sourceType = sd::TextureSourceType::Rgba8;
    texture->m_width = 2;
    texture->m_height = 2;
    const auto truncatedPixels = scene.Validate();
    ASSERT_FALSE(truncatedPixels);
    EXPECT_EQ(truncatedPixels.error().m_code, sd::SceneErrorCode::InvalidTexture);
    texture->m_bytes.resize(16);
    ASSERT_TRUE(scene.Validate());

    material->m_baseColorTexture->m_texture.reset();
    const auto missingTexture = scene.Validate();
    ASSERT_FALSE(missingTexture);
    EXPECT_EQ(missingTexture.error().m_code, sd::SceneErrorCode::InvalidReference);
}

TEST(Scene, TexturedPrimitiveRequiresItsSelectedUvSet)
{
    sd::Scene scene;
    const auto texture = sd::Texture::Create();
    texture->m_path = "normal.png";
    const auto material = sd::Material::Create();
    material->m_normalTexture = sd::TextureBinding{texture, 1};
    const auto mesh = MakeTriangle();
    mesh->m_name = "Panel";
    mesh->m_primitives.push_back({{0, 1, 2}, material});
    mesh->m_texCoords = {{{0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}}};
    const auto node = sd::SceneNode::Create();
    node->m_name = "Door";
    node->m_meshes.push_back(mesh);

    const auto missing = scene.AttachNode(node);
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().m_code, sd::SceneErrorCode::InvalidMesh);
    const auto& message = missing.error().m_message;
    for (const auto* part : {"Door", "Panel", "Primitive 1", "normalTexture", "UV set 1"})
    {
        EXPECT_NE(message.find(part), std::string::npos);
    }

    mesh->m_texCoords.emplace_back();
    const auto empty = scene.AttachNode(node);
    ASSERT_FALSE(empty);
    EXPECT_EQ(empty.error().m_code, sd::SceneErrorCode::InvalidMesh);
    mesh->m_texCoords[1] = mesh->m_texCoords[0];
    ASSERT_TRUE(scene.AttachNode(node));
    EXPECT_TRUE(scene.Validate());
}

TEST(Scene, WorldTransformUpdatePublishesOnlyAfterEntireSceneSucceeds)
{
    sd::Scene scene;
    const auto root = scene.AddNode();
    ASSERT_TRUE(root);
    const auto child = scene.AddNode(root.value());
    ASSERT_TRUE(child);
    EXPECT_FALSE(root.value()->GetCachedWorldTransform());
    ASSERT_TRUE(scene.UpdateWorldTransforms());
    root.value()->m_localTransform(0, 3) = 3.0f;
    child.value()->m_localTransform(0, 3) = 2.0f;
    child.value()->m_localTransform(3, 1) = 1.0f;

    const auto currentRoot = scene.GetWorldTransform(root.value());
    ASSERT_TRUE(currentRoot);
    EXPECT_FLOAT_EQ(currentRoot.value()(0, 3), 3.0f);
    const auto currentChild = scene.GetWorldTransform(child.value());
    ASSERT_FALSE(currentChild);
    EXPECT_EQ(currentChild.error().m_code, sd::SceneErrorCode::InvalidTransform);
    const auto update = scene.UpdateWorldTransforms();
    ASSERT_FALSE(update);
    EXPECT_EQ(update.error().m_code, sd::SceneErrorCode::InvalidTransform);
    ASSERT_TRUE(root.value()->GetCachedWorldTransform());
    ASSERT_TRUE(child.value()->GetCachedWorldTransform());
    EXPECT_TRUE(root.value()->GetCachedWorldTransform()->isIdentity());
    EXPECT_TRUE(child.value()->GetCachedWorldTransform()->isIdentity());

    child.value()->m_localTransform(3, 1) = 0.0f;
    ASSERT_TRUE(scene.UpdateWorldTransforms());
    EXPECT_FLOAT_EQ((*root.value()->GetCachedWorldTransform())(0, 3), 3.0f);
    EXPECT_FLOAT_EQ((*child.value()->GetCachedWorldTransform())(0, 3), 5.0f);
}

TEST(Scene, ReparentingPreservesWorldOrLocalAndUpdatesHierarchy)
{
    sd::Scene scene;
    const auto from = sd::SceneNode::Create();
    from->m_localTransform = rad::ScaleMatrix(rad::Vector3f{-2.0f, 3.0f, 1.0f});
    from->m_localTransform(0, 1) = 0.5f;
    ASSERT_TRUE(scene.AttachNode(from));
    const auto to = sd::SceneNode::Create();
    to->m_localTransform = rad::TranslationMatrix(rad::Vector3f{3.0f, 4.0f, 0.0f});
    to->m_localTransform(0, 0) = 0.5f;
    to->m_localTransform(1, 2) = -0.25f;
    ASSERT_TRUE(scene.AttachNode(to));
    const auto child = sd::SceneNode::Create();
    child->m_localTransform = rad::TranslationMatrix(rad::Vector3f{1.0f, 2.0f, 3.0f});
    ASSERT_TRUE(scene.AttachNode(child, from));
    const auto grandchild = scene.AddNode(child);
    ASSERT_TRUE(grandchild);
    const auto before = scene.GetWorldTransform(grandchild.value());
    ASSERT_TRUE(before);
    const rad::Vector3f position = before.value().topRightCorner<3, 1>();
    EXPECT_TRUE(position.isApprox(rad::Vector3f{-1.0f, 6.0f, 3.0f}));

    ASSERT_TRUE(scene.ReparentNode(child, to, sd::Scene::ReparentMode::KeepWorld));
    EXPECT_TRUE(from->GetChildren().empty());
    ASSERT_EQ(to->GetChildren().size(), 1u);
    EXPECT_EQ(to->GetChildren().front(), child);
    EXPECT_EQ(child->GetParent(), to.get());
    ASSERT_TRUE(scene.UpdateWorldTransforms());
    ASSERT_TRUE(grandchild.value()->GetCachedWorldTransform());
    EXPECT_TRUE(grandchild.value()->GetCachedWorldTransform()->isApprox(before.value()));

    const auto local = child->m_localTransform;
    ASSERT_TRUE(scene.ReparentNode(child, {}));
    const auto rootWorld = scene.GetWorldTransform(child);
    ASSERT_TRUE(rootWorld);
    EXPECT_TRUE(rootWorld.value().isApprox(local));
    EXPECT_EQ(scene.GetRoots().back(), child);
    ASSERT_TRUE(scene.UpdateWorldTransforms());
    ASSERT_TRUE(child->GetCachedWorldTransform());
    EXPECT_TRUE(child->GetCachedWorldTransform()->isApprox(local));
}

TEST(Scene, RejectedReparentKeepsTheOriginalHierarchyAndPose)
{
    sd::Scene scene;
    const auto root = scene.AddNode();
    ASSERT_TRUE(root);
    const auto child = scene.AddNode(root.value());
    ASSERT_TRUE(child);
    child.value()->m_localTransform(0, 3) = 1.0f;

    const auto cycle = scene.ReparentNode(root.value(), child.value());
    ASSERT_FALSE(cycle);
    EXPECT_EQ(cycle.error().m_code, sd::SceneErrorCode::HierarchyCycle);
    EXPECT_FALSE(root.value()->GetParent());

    const auto singularParent = sd::SceneNode::Create();
    singularParent->m_localTransform(0, 0) = 0.0f;
    ASSERT_TRUE(scene.AttachNode(singularParent));
    const auto distantParent = sd::SceneNode::Create();
    distantParent->m_localTransform(0, 3) = 1.0e10f;
    ASSERT_TRUE(scene.AttachNode(distantParent));

    for (const auto& parent : {singularParent, distantParent})
    {
        const auto result = scene.ReparentNode(child.value(), parent, sd::Scene::ReparentMode::KeepWorld);
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().m_code, sd::SceneErrorCode::InvalidTransform);
        EXPECT_EQ(child.value()->GetParent(), root.value().get());
        ASSERT_EQ(root.value()->GetChildren().size(), 1u);
        EXPECT_EQ(root.value()->GetChildren().front(), child.value());
        EXPECT_TRUE(parent->GetChildren().empty());
        EXPECT_FLOAT_EQ(child.value()->m_localTransform(0, 3), 1.0f);
    }
}

TEST(Scene, ValidatesProjectionAfterAnEditorSwitchesCameraType)
{
    sd::Scene scene;
    const auto node = sd::SceneNode::Create();
    auto& camera = node->m_camera.emplace();
    camera.m_farPlane = std::numeric_limits<float>::infinity();
    camera.m_nearPlane = 0.25f;
    camera.m_aspectRatio = 1.5f;
    camera.m_orthographicHeight = 0.0f; // Ignored for perspective.
    ASSERT_TRUE(scene.AttachNode(node));

    camera.m_type = sd::CameraType::Orthographic;
    camera.m_orthographicHeight = 4.0f;
    const auto infiniteOrtho = scene.Validate();
    ASSERT_FALSE(infiniteOrtho);
    EXPECT_EQ(infiniteOrtho.error().m_code, sd::SceneErrorCode::InvalidCamera);
    camera.m_farPlane = 200.0f;
    camera.m_verticalFov = std::numeric_limits<float>::quiet_NaN(); // Ignored for orthographic.
    camera.m_aspectRatio = 0.0f;
    const auto invalidAspect = scene.Validate();
    ASSERT_FALSE(invalidAspect);
    EXPECT_EQ(invalidAspect.error().m_code, sd::SceneErrorCode::InvalidCamera);
    camera.m_aspectRatio = 1.5f;
    EXPECT_TRUE(scene.Validate());
}

TEST(Camera, ProjectsFiniteAndInfinitePerspectiveDepthRanges)
{
    sd::Camera camera;
    camera.m_verticalFov = std::numbers::pi_v<float> / 2.0f;
    camera.m_aspectRatio = 2.0f;
    camera.m_nearPlane = 1.0f;
    for (const auto depth : {rad::ClipDepth::ZeroToOne, rad::ClipDepth::NegativeOneToOne})
    {
        const float nearDepth = depth == rad::ClipDepth::ZeroToOne ? 0.0f : -1.0f;
        camera.m_farPlane = 100.0f;
        const auto finite = camera.ComputeProjection(depth);
        ASSERT_TRUE(finite);
        const auto nearCorner = rad::ProjectToNdc(finite.value(), rad::Vector3f{2.0f, 1.0f, -1.0f});
        EXPECT_TRUE(nearCorner.isApprox(rad::Vector3f{1.0f, 1.0f, nearDepth}));
        const auto far = rad::ProjectToNdc(finite.value(), rad::Vector3f{0.0f, 0.0f, -100.0f});
        EXPECT_NEAR(far.z(), 1.0f, 1e-6f);

        camera.m_farPlane = std::numeric_limits<float>::infinity();
        const auto infinite = camera.ComputeProjection(depth);
        ASSERT_TRUE(infinite);
        const auto near = rad::ProjectToNdc(infinite.value(), rad::Vector3f{0.0f, 0.0f, -1.0f});
        const auto distant = rad::ProjectToNdc(infinite.value(), rad::Vector3f{0.0f, 0.0f, -100000.0f});
        EXPECT_NEAR(near.z(), nearDepth, 1e-6f);
        EXPECT_LT(distant.z(), 1.0f);
        EXPECT_GT(distant.z(), 0.9999f);
    }
}

TEST(Camera, ProjectsOrthographicExtentsAndRejectsUnrepresentableMatrices)
{
    sd::Camera camera;
    camera.m_type = sd::CameraType::Orthographic;
    camera.m_aspectRatio = 2.0f;
    camera.m_orthographicHeight = 4.0f;
    camera.m_nearPlane = 0.0f;
    camera.m_farPlane = 10.0f;
    for (const auto depth : {rad::ClipDepth::ZeroToOne, rad::ClipDepth::NegativeOneToOne})
    {
        const auto projection = camera.ComputeProjection(depth);
        ASSERT_TRUE(projection);
        const float nearDepth = depth == rad::ClipDepth::ZeroToOne ? 0.0f : -1.0f;
        const auto lower = rad::ProjectToNdc(projection.value(), rad::Vector3f{-4.0f, -2.0f, 0.0f});
        const auto upper = rad::ProjectToNdc(projection.value(), rad::Vector3f{4.0f, 2.0f, -10.0f});
        EXPECT_TRUE(lower.isApprox(rad::Vector3f{-1.0f, -1.0f, nearDepth}));
        EXPECT_TRUE(upper.isApprox(rad::Vector3f::Ones()));
    }

    camera.m_orthographicHeight = std::numeric_limits<float>::min() / 4.0f;
    const auto projection = camera.ComputeProjection(rad::ClipDepth::ZeroToOne);
    ASSERT_FALSE(projection);
    EXPECT_EQ(projection.error(), std::errc::result_out_of_range);
}

TEST(Scene, ValidatesRangeAndConeAsLightTypeChanges)
{
    sd::Scene scene;
    const auto node = sd::SceneNode::Create();
    auto& light = node->m_light.emplace();
    light.m_range = 0.0f; // Ignored for directional lights.
    light.m_innerConeAngle = 1.0f; // Ignored for directional and point lights.
    ASSERT_TRUE(scene.AttachNode(node));

    light.m_type = sd::LightType::Point;
    const auto invalidRange = scene.Validate();
    ASSERT_FALSE(invalidRange);
    EXPECT_EQ(invalidRange.error().m_code, sd::SceneErrorCode::InvalidLight);
    light.m_range = std::numeric_limits<float>::infinity();
    ASSERT_TRUE(scene.Validate());

    light.m_type = sd::LightType::Spot;
    const auto invalidCone = scene.Validate();
    ASSERT_FALSE(invalidCone);
    EXPECT_EQ(invalidCone.error().m_code, sd::SceneErrorCode::InvalidLight);
    light.m_innerConeAngle = 0.25f;
    EXPECT_TRUE(scene.Validate());
}

TEST(Scene, DetachingSubtreeUpdatesMembershipAndAllowsReattachment)
{
    sd::Scene source;
    sd::Scene destination;
    const auto root = source.AddNode();
    ASSERT_TRUE(root);
    const auto child = source.AddNode(root.value());
    ASSERT_TRUE(child);
    const auto grandchild = source.AddNode(child.value());
    ASSERT_TRUE(grandchild);
    const auto newRoot = destination.AddNode();
    ASSERT_TRUE(newRoot);

    const auto alreadyAttached = destination.AttachNode(child.value(), newRoot.value());
    ASSERT_FALSE(alreadyAttached);
    EXPECT_EQ(alreadyAttached.error().m_code, sd::SceneErrorCode::InvalidReference);
    EXPECT_FALSE(source.ReparentNode(child.value(), newRoot.value()));

    ASSERT_TRUE(source.DetachNode(root.value()->GetChildren().front()));
    EXPECT_TRUE(root.value()->GetChildren().empty());
    EXPECT_EQ(child.value()->GetParent(), nullptr);
    EXPECT_EQ(grandchild.value()->GetParent(), child.value().get());
    EXPECT_FALSE(source.GetWorldTransform(grandchild.value()));

    ASSERT_TRUE(destination.AttachNode(child.value(), newRoot.value()));
    EXPECT_EQ(child.value()->GetParent(), newRoot.value().get());
    EXPECT_TRUE(destination.GetWorldTransform(grandchild.value()));
}

TEST(Scene, RetainedChildOutlivesParentAndKeepsAssetChain)
{
    rad::Ref<sd::SceneNode> child;
    {
        const auto scene = sd::Scene::Create();
        const auto root = scene->AddNode();
        ASSERT_TRUE(root);
        auto mesh = MakeTriangle();
        mesh->m_texCoords = {{{0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}}};
        auto material = sd::Material::Create();
        auto texture = sd::Texture::Create();
        texture->m_path = "checker.png";
        material->m_baseColorTexture = sd::TextureBinding{std::move(texture)};
        mesh->m_primitives.front().m_material = std::move(material);
        child = sd::SceneNode::Create();
        child->m_meshes.push_back(std::move(mesh));
        ASSERT_TRUE(scene->AttachNode(child, root.value()));
        EXPECT_EQ(child->GetParent(), root.value().get());
    }

    ASSERT_TRUE(child);
    EXPECT_EQ(child->GetParent(), nullptr);
    const auto mesh = child->m_meshes.front();
    child.reset();
    const auto& binding = mesh->m_primitives.front().m_material->m_baseColorTexture;
    ASSERT_TRUE(binding);
    ASSERT_TRUE(binding->m_texture);
    EXPECT_EQ(binding->m_texture->m_path, rad::FilePath{"checker.png"});
}

TEST(Scene, RetainedRootKeepsItsSubtreeAndCanJoinANewScene)
{
    rad::Ref<sd::SceneNode> root;
    {
        sd::Scene scene;
        const auto result = scene.AddNode();
        ASSERT_TRUE(result);
        root = result.value();
        ASSERT_TRUE(scene.AddNode(root));
    }
    ASSERT_EQ(root->GetChildren().size(), 1u);
    const auto child = root->GetChildren().front();
    EXPECT_EQ(child->GetParent(), root.get());

    sd::Scene scene;
    ASSERT_TRUE(scene.AttachNode(root));
    EXPECT_TRUE(scene.GetWorldTransform(child));
}

TEST(Scene, ReleasesADeepTreeWithoutDestroyingAnExternallyRetainedLeaf)
{
    constexpr int depth = 8192;
    rad::Ref<sd::SceneNode> leaf;
    {
        sd::Scene scene;
        for (int i = 0; i < depth; ++i)
        {
            auto node = scene.AddNode(leaf);
            ASSERT_TRUE(node);
            leaf = std::move(node).value();
        }
        ASSERT_NE(leaf->GetParent(), nullptr);
    }
    EXPECT_EQ(leaf->GetParent(), nullptr);
}

} // namespace
