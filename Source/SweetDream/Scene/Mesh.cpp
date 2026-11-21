#include <SweetDream/Scene/Mesh.h>

namespace sd
{

std::optional<AxisAlignedBounds> Mesh::ComputeBounds() const
{
    if (m_positions.empty())
    {
        return std::nullopt;
    }

    AxisAlignedBounds bounds{m_positions.front(), m_positions.front()};
    for (const auto& position : m_positions)
    {
        if (!position.allFinite())
        {
            return std::nullopt;
        }
        bounds.m_min = bounds.m_min.cwiseMin(position);
        bounds.m_max = bounds.m_max.cwiseMax(position);
    }
    return bounds;
}

bool Mesh::UpdateBounds()
{
    m_bounds = ComputeBounds();
    return m_bounds.has_value();
}

} // namespace sd
