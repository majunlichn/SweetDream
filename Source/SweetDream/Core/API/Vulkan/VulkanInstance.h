#pragma once

#include <SweetDream/Core/Common/String.h>
#include <SweetDream/Core/API/Vulkan/VulkanCommon.h>

namespace sd
{

class VulkanInstance : public rad::RefCounted<VulkanInstance>
{
public:
    static rad::Ref<VulkanInstance> Create(std::string_view appName, uint32_t appVersion,
                                           std::string_view engineName, uint32_t engineVersion,
                                           const sd::StringSet& requiredExtensions = {});

    ~VulkanInstance();

    VulkanInstance(const VulkanInstance&) = delete;
    VulkanInstance& operator=(const VulkanInstance&) = delete;

    [[nodiscard]] const vk::Instance& GetHandle() const noexcept { return m_handle; }
    [[nodiscard]] const vk::detail::DispatchLoaderDynamic& GetDispatcher() const noexcept
    {
        return m_dispatcher;
    }

    [[nodiscard]] PFN_vkVoidFunction GetProcAddr(const char* name) const
    {
        return m_handle.getProcAddr(name, m_dispatcher);
    }

    [[nodiscard]] std::vector<vk::LayerProperties> EnumerateInstanceLayers() const;
    [[nodiscard]] std::vector<vk::ExtensionProperties> EnumerateInstanceExtensions(
        vk::Optional<const std::string> layerName = nullptr) const;

    [[nodiscard]] VulkanVersion GetApiVersion() const noexcept { return m_apiVersion; }

    [[nodiscard]] bool IsLayerEnabled(std::string_view name) const
    {
        return m_enabledLayers.find(name) != m_enabledLayers.end();
    }

    [[nodiscard]] bool IsExtensionEnabled(std::string_view name) const
    {
        return m_enabledExtensions.find(name) != m_enabledExtensions.end();
    }

    [[nodiscard]] const std::vector<vk::PhysicalDevice>& GetPhysicalDevices() const noexcept
    {
        return m_physicalDevices;
    }

private:
    VulkanInstance();

    bool Init(std::string_view appName, uint32_t appVersion, std::string_view engineName,
              uint32_t engineVersion, const sd::StringSet& requiredExtensions);

    vk::detail::DynamicLoader m_loader;
    vk::detail::DispatchLoaderDynamic m_dispatcher;

    vk::Instance m_handle = nullptr;
    VulkanVersion m_apiVersion = VK_API_VERSION_1_0;
    sd::StringSet m_enabledLayers;
    sd::StringSet m_enabledExtensions;

    vk::DebugUtilsMessengerEXT m_debugUtilsMessenger = nullptr;
    std::vector<vk::PhysicalDevice> m_physicalDevices;
}; // class VulkanInstance

} // namespace sd
