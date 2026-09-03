#include <SweetDream/Core/API/Vulkan/VulkanSurface.h>
#include <SweetDream/Core/API/Vulkan/VulkanInstance.h>

#include <stdexcept>
#include <utility>

namespace sd
{

rad::Ref<VulkanSurface> VulkanSurface::Create(rad::Ref<VulkanInstance> instance,
                                              vk::SurfaceKHR surface)
{
    return rad::Ref<VulkanSurface>{new VulkanSurface(std::move(instance), surface)};
}

VulkanSurface::VulkanSurface(
    rad::Ref<VulkanInstance> instance,
    const vk::DisplaySurfaceCreateInfoKHR& createInfo) :
    m_instance(std::move(instance))
{
    if (!m_instance)
    {
        throw std::invalid_argument("VulkanSurface requires a valid VulkanInstance");
    }

    m_handle =
        m_instance->GetHandle().createDisplayPlaneSurfaceKHR(createInfo, nullptr, GetDispatcher());
}

VulkanSurface::VulkanSurface(rad::Ref<VulkanInstance> instance, vk::SurfaceKHR surface) :
    m_instance(std::move(instance)),
    m_handle(surface)
{
    if (!m_instance)
    {
        throw std::invalid_argument("VulkanSurface requires a valid VulkanInstance");
    }
    if (!m_handle)
    {
        throw std::invalid_argument("VulkanSurface requires a valid surface handle");
    }
}

VulkanSurface::~VulkanSurface()
{
    if (m_handle)
    {
        m_instance->GetHandle().destroySurfaceKHR(m_handle, nullptr, GetDispatcher());
        m_handle = nullptr;
    }
}

const vk::detail::DispatchLoaderDynamic& VulkanSurface::GetDispatcher() const noexcept
{
    return m_instance->GetDispatcher();
}

} // namespace sd
