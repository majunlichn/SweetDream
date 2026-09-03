#include <SweetDream/Core/API/Vulkan/VulkanDevice.h>
#include <SweetDream/Core/API/Vulkan/VulkanInstance.h>
#include <SweetDream/Core/IO/Logging.h>

#include <rad/System/Application.h>

#include <cstddef>
#include <cstdlib>
#include <exception>
#include <stdexcept>

int main(int argc, char** argv)
{
    try
    {
        rad::Application::Instance().Init(argc, argv);

        rad::Ref<sd::VulkanInstance> instance = sd::VulkanInstance::Create(
            "HelloTriangle", VK_MAKE_API_VERSION(0, 1, 0, 0), "SweetDream",
            VK_MAKE_API_VERSION(0, 0, 0, 0));
        if (!instance)
        {
            throw std::runtime_error("Failed to create Vulkan instance");
        }

        const sd::VulkanVersion apiVersion = instance->GetApiVersion();
        SD_LOG(info, "Instance API version: {}", apiVersion.ToString());

        const auto& physicalDevices = instance->GetPhysicalDevices();
        for (std::size_t i = 0; i < physicalDevices.size(); ++i)
        {
            const vk::PhysicalDeviceProperties properties =
                physicalDevices[i].getProperties(instance->GetDispatcher());
            SD_LOG(info, "Device #{}: {} (0x{:04X})", i, properties.deviceName.data(),
                   properties.deviceID);
        }

        if (physicalDevices.empty())
        {
            throw std::runtime_error("No Vulkan physical devices are available");
        }

        rad::Ref<sd::VulkanDevice> device{
            new sd::VulkanDevice(instance, physicalDevices.front())};
        if (!device || !device->GetHandle() || device->GetAllocator() == nullptr)
        {
            throw std::runtime_error("Vulkan device initialization is incomplete");
        }

        SD_LOG(info, "Logical device created on '{}'", device->GetName());
        device->WaitIdle();
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        try
        {
            SD_LOG(err, "HelloTriangle failed: {}", error.what());
        }
        catch (...)
        {
        }
        return EXIT_FAILURE;
    }
}
