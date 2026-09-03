#define VMA_IMPLEMENTATION
#include <SweetDream/Core/API/Vulkan/VulkanCommon.h>

#include <algorithm>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace sd
{

std::string VulkanVersion::ToString() const
{
    return std::to_string(GetMajor()) + "." + std::to_string(GetMinor()) + "." +
           std::to_string(GetPatch());
}

bool Contains(const std::vector<vk::LayerProperties>& properties, std::string_view name)
{
    return std::ranges::any_of(properties, [name](const vk::LayerProperties& property)
                               { return name == property.layerName.data(); });
}

bool Contains(const std::vector<vk::ExtensionProperties>& properties, std::string_view name)
{
    return std::ranges::any_of(properties, [name](const vk::ExtensionProperties& property)
                               { return name == property.extensionName.data(); });
}

} // namespace sd
