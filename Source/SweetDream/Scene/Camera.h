#pragma once

#include <rad/Math/Transform3D.h>

#include <numbers>

namespace sd
{

enum class CameraType
{
    Perspective,
    Orthographic
};

class Camera
{
public:
    CameraType m_type = CameraType::Perspective;
    float m_nearPlane = 0.1f;
    float m_farPlane = 1000.0f; // Positive infinity is allowed only for perspective.
    float m_aspectRatio = 16.0f / 9.0f; // Projection width divided by height.
    float m_verticalFov = std::numbers::pi_v<float> / 3.0f; // Perspective only, in radians.
    float m_orthographicHeight = 2.0f; // Full vertical span, in meters.

    // Right-handed view space (-Z forward), with no Y flip or reversed depth.
    [[nodiscard]] rad::Result<rad::Matrix4f> ComputeProjection(rad::ClipDepth depth) const;
}; // class Camera

} // namespace sd
