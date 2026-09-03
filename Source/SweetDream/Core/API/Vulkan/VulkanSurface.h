#pragma once

#include <SweetDream/Core/API/Vulkan/VulkanCommon.h>

namespace sd
{

class VulkanInstance;

class VulkanSurface : public rad::RefCounted<VulkanSurface>
{
public:
    static rad::Ref<VulkanSurface> Create(rad::Ref<VulkanInstance> instance,
                                          vk::SurfaceKHR surface);

    VulkanSurface(rad::Ref<VulkanInstance> instance,
                  const vk::DisplaySurfaceCreateInfoKHR& createInfo);
    VulkanSurface(rad::Ref<VulkanInstance> instance, vk::SurfaceKHR surface);
    ~VulkanSurface();

    VulkanSurface(const VulkanSurface&) = delete;
    VulkanSurface& operator=(const VulkanSurface&) = delete;

    [[nodiscard]] VulkanInstance* GetInstance() const noexcept { return m_instance.get(); }
    [[nodiscard]] const vk::detail::DispatchLoaderDynamic& GetDispatcher() const noexcept;
    [[nodiscard]] const vk::SurfaceKHR& GetHandle() const noexcept { return m_handle; }

private:
    rad::Ref<VulkanInstance> m_instance;
    vk::SurfaceKHR m_handle = nullptr;
}; // class VulkanSurface

} // namespace sd
