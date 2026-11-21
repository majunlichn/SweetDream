#include <SweetDream/Scene/Camera.h>

#include <cmath>
#include <limits>
#include <system_error>
#include <utility>

namespace sd
{

rad::Result<rad::Matrix4f> Camera::ComputeProjection(rad::ClipDepth depth) const
{
    if ((depth != rad::ClipDepth::ZeroToOne && depth != rad::ClipDepth::NegativeOneToOne) ||
        !std::isfinite(m_aspectRatio) || m_aspectRatio <= 0.0f ||
        !std::isfinite(m_nearPlane) || !(m_farPlane > m_nearPlane))
    {
        return rad::Failure(std::errc::invalid_argument);
    }

    const double nearPlane = m_nearPlane;
    const double farPlane = m_farPlane;
    const double aspect = m_aspectRatio;
    rad::Matrix4d projection;
    try
    {
        switch (m_type)
        {
        case CameraType::Perspective:
            if (m_nearPlane <= 0.0f ||
                !(m_verticalFov > 0.0f && m_verticalFov < std::numbers::pi_v<float>))
            {
                return rad::Failure(std::errc::invalid_argument);
            }
            if (std::isinf(farPlane))
            {
                // Limit of the perspective matrix as the far plane tends to infinity.
                const double cotangent = 1.0 / std::tan(static_cast<double>(m_verticalFov) / 2.0);
                projection.setZero();
                projection(0, 0) = cotangent / aspect;
                projection(1, 1) = cotangent;
                projection(2, 2) = -1.0;
                projection(2, 3) = depth == rad::ClipDepth::ZeroToOne ? -nearPlane : -2.0 * nearPlane;
                projection(3, 2) = -1.0;
            }
            else
            {
                projection = rad::PerspectiveRH(static_cast<double>(m_verticalFov), aspect,
                                                nearPlane, farPlane, depth);
            }
            break;
        case CameraType::Orthographic:
        {
            if (m_nearPlane < 0.0f || !std::isfinite(farPlane) ||
                !std::isfinite(m_orthographicHeight) || m_orthographicHeight <= 0.0f)
            {
                return rad::Failure(std::errc::invalid_argument);
            }
            const double halfHeight = static_cast<double>(m_orthographicHeight) / 2.0;
            const double halfWidth = halfHeight * aspect;
            projection = rad::OrthographicRH(-halfWidth, halfWidth, -halfHeight, halfHeight,
                                              nearPlane, farPlane, depth);
            break;
        }
        default:
            return rad::Failure(std::errc::invalid_argument);
        }
    }
    catch (const std::system_error& error)
    {
        return rad::Failure(error.code());
    }

    if (!projection.allFinite() || projection.cwiseAbs().maxCoeff() > std::numeric_limits<float>::max())
    {
        return rad::Failure(std::errc::result_out_of_range);
    }
    rad::Matrix4f result = projection.cast<float>();
    if (result(0, 0) == 0.0f || result(1, 1) == 0.0f || result(2, 2) == 0.0f ||
        (m_type == CameraType::Perspective && result(2, 3) == 0.0f))
    {
        return rad::Failure(std::errc::result_out_of_range);
    }
    return rad::Success(std::move(result));
}

} // namespace sd
