#include <SweetDream/Core/API/Vulkan/VulkanInstance.h>

#include <rad/System/OS.h>

#include <exception>
#include <utility>

namespace sd
{

namespace
{

constexpr const char* ValidationLayerName = "VK_LAYER_KHRONOS_validation";

VKAPI_ATTR vk::Bool32 VKAPI_CALL VulkanDebugUtilsMessengerCallback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    vk::DebugUtilsMessageTypeFlagsEXT messageTypes,
    const vk::DebugUtilsMessengerCallbackDataEXT* callbackData, void* userData) noexcept
{
    (void)messageTypes;
    (void)userData;

    const char* message = callbackData != nullptr ? callbackData->pMessage : nullptr;
    message = message != nullptr ? message : "Vulkan validation message contained no text";

    try
    {
        switch (messageSeverity)
        {
        case vk::DebugUtilsMessageSeverityFlagBitsEXT::eError:
            SD_LOG(err, "{}", message);
            break;
        case vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning:
            SD_LOG(warn, "{}", message);
            break;
        case vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo:
            SD_LOG(info, "{}", message);
            break;
        default:
            SD_LOG(debug, "{}", message);
            break;
        }
    }
    catch (...)
    {
        // Exceptions must not cross the Vulkan C callback boundary.
    }

    return VK_FALSE;
}

} // namespace

rad::Ref<VulkanInstance> VulkanInstance::Create(std::string_view appName, uint32_t appVersion,
                                                std::string_view engineName, uint32_t engineVersion,
                                                const sd::StringSet& requiredExtensions)
{
    rad::Ref<VulkanInstance> instance{new VulkanInstance};
    if (!instance->Init(appName, appVersion, engineName, engineVersion, requiredExtensions))
    {
        return {};
    }
    return instance;
}

VulkanInstance::VulkanInstance()
{
    const auto getInstanceProcAddr =
        m_loader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
    if (getInstanceProcAddr != nullptr)
    {
        m_dispatcher.init(getInstanceProcAddr);
    }
}

VulkanInstance::~VulkanInstance()
{
    if (m_debugUtilsMessenger)
    {
        m_handle.destroyDebugUtilsMessengerEXT(m_debugUtilsMessenger, nullptr, m_dispatcher);
    }
    if (m_handle)
    {
        m_handle.destroy(nullptr, m_dispatcher);
    }
}

std::vector<vk::LayerProperties> VulkanInstance::EnumerateInstanceLayers() const
{
    return vk::enumerateInstanceLayerProperties(m_dispatcher);
}

std::vector<vk::ExtensionProperties> VulkanInstance::EnumerateInstanceExtensions(
    vk::Optional<const std::string> layerName) const
{
    return vk::enumerateInstanceExtensionProperties(layerName, m_dispatcher);
}

bool VulkanInstance::Init(std::string_view appName, uint32_t appVersion,
                          std::string_view engineName, uint32_t engineVersion,
                          const sd::StringSet& requiredExtensions)
{
    if (m_handle)
    {
        SD_LOG(err, "VulkanInstance is already initialized");
        return false;
    }
    if (m_dispatcher.vkCreateInstance == nullptr)
    {
        SD_LOG(err, "Failed to load vkCreateInstance");
        return false;
    }

    try
    {
        m_enabledExtensions.insert(requiredExtensions.begin(), requiredExtensions.end());

#if defined(NDEBUG)
        bool requestValidationLayer = false;
#else
        bool requestValidationLayer = true;
#endif
        if (const auto value = rad::os::getenv("SD_ENABLE_VALIDATION_LAYER"))
        {
            if (const auto enabled = rad::StrToBool(*value))
            {
                requestValidationLayer = *enabled;
            }
            else
            {
                SD_LOG(warn, "Ignoring invalid SD_ENABLE_VALIDATION_LAYER value: {}", *value);
            }
        }

        if (requestValidationLayer)
        {
            m_enabledLayers.emplace(ValidationLayerName);
            m_enabledExtensions.emplace(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        const auto availableLayers = EnumerateInstanceLayers();
        for (const std::string& layer : m_enabledLayers)
        {
            if (!Contains(availableLayers, layer))
            {
                SD_LOG(err, "Requested Vulkan instance layer is unavailable: {}", layer);
                return false;
            }
        }

        const auto availableExtensions = EnumerateInstanceExtensions();
        if (Contains(availableExtensions, VK_KHR_SURFACE_EXTENSION_NAME))
        {
            m_enabledExtensions.emplace(VK_KHR_SURFACE_EXTENSION_NAME);
        }
        if (Contains(availableExtensions, VK_KHR_DISPLAY_EXTENSION_NAME))
        {
            m_enabledExtensions.emplace(VK_KHR_DISPLAY_EXTENSION_NAME);
        }
        for (const std::string& extension : m_enabledExtensions)
        {
            if (!Contains(availableExtensions, extension))
            {
                SD_LOG(err, "Requested Vulkan instance extension is unavailable: {}", extension);
                return false;
            }
        }

        uint32_t apiVersion = VK_API_VERSION_1_0;
        if (m_dispatcher.vkEnumerateInstanceVersion != nullptr)
        {
            const VkResult result = m_dispatcher.vkEnumerateInstanceVersion(&apiVersion);
            if (result != VK_SUCCESS)
            {
                SD_LOG(err, "vkEnumerateInstanceVersion failed with result {}",
                       static_cast<int>(result));
                return false;
            }
        }
        m_apiVersion = apiVersion;
        if (m_apiVersion.GetVariant() != 0)
        {
            SD_LOG(err, "Unsupported Vulkan API variant: {}", m_apiVersion.GetVariant());
            return false;
        }
        if (m_apiVersion.IsLowerThan(VK_API_VERSION_1_1))
        {
            SD_LOG(err, "Vulkan loader {} is unsupported. Please update the Vulkan runtime.",
                   m_apiVersion.ToString());
            return false;
        }

        const std::string appNameStorage{appName};
        const std::string engineNameStorage{engineName};

        vk::ApplicationInfo appInfo;
        appInfo.pApplicationName = appNameStorage.c_str();
        appInfo.applicationVersion = appVersion;
        appInfo.pEngineName = engineNameStorage.c_str();
        appInfo.engineVersion = engineVersion;
        appInfo.apiVersion = TargetVulkanApiVersion;

        std::vector<const char*> layerNames;
        layerNames.reserve(m_enabledLayers.size());
        for (const std::string& layer : m_enabledLayers)
        {
            layerNames.push_back(layer.c_str());
        }

        std::vector<const char*> extensionNames;
        extensionNames.reserve(m_enabledExtensions.size());
        for (const std::string& extension : m_enabledExtensions)
        {
            extensionNames.push_back(extension.c_str());
        }

        vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo;
        debugCreateInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                                          vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
                                          vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                          vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
        debugCreateInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                      vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                                      vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
        debugCreateInfo.pfnUserCallback = VulkanDebugUtilsMessengerCallback;

        vk::InstanceCreateInfo createInfo;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledLayerCount = static_cast<uint32_t>(layerNames.size());
        createInfo.ppEnabledLayerNames = layerNames.data();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensionNames.size());
        createInfo.ppEnabledExtensionNames = extensionNames.data();
        if (requestValidationLayer)
        {
            createInfo.pNext = &debugCreateInfo;
        }

        m_handle = vk::createInstance(createInfo, nullptr, m_dispatcher);
        m_dispatcher.init(m_handle);

        if (requestValidationLayer)
        {
            m_debugUtilsMessenger =
                m_handle.createDebugUtilsMessengerEXT(debugCreateInfo, nullptr, m_dispatcher);
        }

        m_physicalDevices = m_handle.enumeratePhysicalDevices(m_dispatcher);
        return true;
    }
    catch (const std::exception& exception)
    {
        SD_LOG(err, "Failed to initialize Vulkan instance: {}", exception.what());

        if (m_debugUtilsMessenger)
        {
            m_handle.destroyDebugUtilsMessengerEXT(m_debugUtilsMessenger, nullptr, m_dispatcher);
            m_debugUtilsMessenger = nullptr;
        }
        if (m_handle)
        {
            m_handle.destroy(nullptr, m_dispatcher);
            m_handle = nullptr;
        }
        m_physicalDevices.clear();
        return false;
    }
}

} // namespace sd
