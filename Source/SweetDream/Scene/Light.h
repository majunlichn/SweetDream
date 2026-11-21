#pragma once

#include <rad/Math/Vector.h>

#include <limits>
#include <numbers>

namespace sd
{

enum class LightType
{
    Directional,
    Point,
    Spot
};

class Light
{
public:
    LightType m_type = LightType::Directional;
    rad::Vector3f m_color = rad::Vector3f::Ones(); // Linear RGB.
    float m_intensity = 1.0f; // Lux for directional; candela for point and spot.
    // Point and spot only, in meters. Positive infinity means no distance cutoff.
    float m_range = std::numeric_limits<float>::infinity();
    // Spot only, in radians from the center axis.
    float m_innerConeAngle = 0.0f;
    float m_outerConeAngle = std::numbers::pi_v<float> / 4.0f;
}; // class Light

} // namespace sd
