#include <SweetDream/Core/API/Vulkan/VulkanImage.h>
#include <SweetDream/Core/API/Vulkan/VulkanDevice.h>

#include <stdexcept>
#include <utility>

namespace sd
{

namespace
{
vk::ImageAspectFlags GetDefaultAspectMask(vk::Format format)
{
    switch (format)
    {
    case vk::Format::eD16Unorm:
    case vk::Format::eX8D24UnormPack32:
    case vk::Format::eD32Sfloat:
        return vk::ImageAspectFlagBits::eDepth;

    case vk::Format::eS8Uint:
        return vk::ImageAspectFlagBits::eStencil;

    case vk::Format::eD16UnormS8Uint:
    case vk::Format::eD24UnormS8Uint:
    case vk::Format::eD32SfloatS8Uint:
        return vk::ImageAspectFlagBits::eDepth |
               vk::ImageAspectFlagBits::eStencil;

    default:
        return vk::ImageAspectFlagBits::eColor;
    }
}

void ValidateSubresourceRange(const vk::ImageSubresourceRange& range,
                              uint32_t mipLevels, uint32_t arrayLayers)
{
    if (!range.aspectMask)
    {
        throw std::invalid_argument("VulkanImageView requires a non-empty aspect mask");
    }
    if (range.baseMipLevel >= mipLevels || range.levelCount == 0 ||
        (range.levelCount != VK_REMAINING_MIP_LEVELS &&
         range.levelCount > mipLevels - range.baseMipLevel))
    {
        throw std::out_of_range("VulkanImageView mip range is outside the image");
    }
    if (range.baseArrayLayer >= arrayLayers || range.layerCount == 0 ||
        (range.layerCount != VK_REMAINING_ARRAY_LAYERS &&
         range.layerCount > arrayLayers - range.baseArrayLayer))
    {
        throw std::out_of_range("VulkanImageView array range is outside the image");
    }
}
} // namespace

VulkanImage::VulkanImage(rad::Ref<VulkanDevice> device,
                         const vk::ImageCreateInfo& createInfo,
                         const VmaAllocationCreateInfo& allocationCreateInfo) :
    m_device(std::move(device)),
    m_flags(createInfo.flags),
    m_imageType(createInfo.imageType),
    m_format(createInfo.format),
    m_extent(createInfo.extent),
    m_mipLevels(createInfo.mipLevels),
    m_arrayLayers(createInfo.arrayLayers),
    m_samples(createInfo.samples),
    m_tiling(createInfo.tiling),
    m_usage(createInfo.usage),
    m_sharingMode(createInfo.sharingMode)
{
    if (!m_device)
    {
        throw std::invalid_argument("VulkanImage requires a valid VulkanDevice");
    }

    VkImage image = VK_NULL_HANDLE;
    SD_CHECK_VKRESULT(vmaCreateImage(
        m_device->GetAllocator(), reinterpret_cast<const VkImageCreateInfo*>(&createInfo),
        &allocationCreateInfo, &image, &m_alloc, &m_allocInfo));
    m_handle = image;

    VkMemoryPropertyFlags memoryPropertyFlags = 0;
    vmaGetAllocationMemoryProperties(m_device->GetAllocator(), m_alloc,
                                     &memoryPropertyFlags);
    m_memPropFlags = vk::MemoryPropertyFlags{memoryPropertyFlags};
}

VulkanImage::VulkanImage(rad::Ref<VulkanDevice> device,
                         const vk::ImageCreateInfo& createInfo, vk::Image handle) :
    m_device(std::move(device)),
    m_handle(handle),
    m_flags(createInfo.flags),
    m_imageType(createInfo.imageType),
    m_format(createInfo.format),
    m_extent(createInfo.extent),
    m_mipLevels(createInfo.mipLevels),
    m_arrayLayers(createInfo.arrayLayers),
    m_samples(createInfo.samples),
    m_tiling(createInfo.tiling),
    m_usage(createInfo.usage),
    m_sharingMode(createInfo.sharingMode)
{
    if (!m_device)
    {
        throw std::invalid_argument("VulkanImage requires a valid VulkanDevice");
    }
    if (!m_handle)
    {
        throw std::invalid_argument("VulkanImage requires a valid image handle");
    }
}

rad::Ref<VulkanImage> VulkanImage::Create(
    rad::Ref<VulkanDevice> device, const vk::ImageCreateInfo& createInfo,
    const VmaAllocationCreateInfo& allocationCreateInfo)
{
    return rad::Ref<VulkanImage>{
        new VulkanImage(std::move(device), createInfo, allocationCreateInfo)};
}

rad::Ref<VulkanImage> VulkanImage::WrapBorrowed(
    rad::Ref<VulkanDevice> device, const vk::ImageCreateInfo& createInfo,
    vk::Image handle)
{
    return rad::Ref<VulkanImage>{
        new VulkanImage(std::move(device), createInfo, handle)};
}

VulkanImage::~VulkanImage()
{
    // Images supplied through the handle constructor are non-owning.
    if (m_handle && m_alloc)
    {
        vmaDestroyImage(m_device->GetAllocator(), m_handle, m_alloc);
        m_handle = nullptr;
        m_alloc = nullptr;
        m_allocInfo = {};
    }
}

const vk::detail::DispatchLoaderDynamic& VulkanImage::GetDispatcher() const noexcept
{
    return m_device->GetDispatcher();
}

rad::Ref<VulkanImageView> VulkanImage::CreateView(
    vk::ImageViewType type, vk::Format format,
    const vk::ImageSubresourceRange& range, vk::ComponentMapping components)
{
    ValidateSubresourceRange(range, m_mipLevels, m_arrayLayers);

    vk::ImageViewCreateInfo createInfo;
    createInfo.image = m_handle;
    createInfo.viewType = type;
    createInfo.format = format;
    createInfo.components = components;
    createInfo.subresourceRange = range;

    return rad::Ref<VulkanImageView>{
        new VulkanImageView(rad::Ref<VulkanImage>{this}, createInfo)};
}

rad::Ref<VulkanImageView> VulkanImage::CreateView(vk::ImageViewType type,
                                                  vk::Format format)
{
    vk::ImageSubresourceRange range;
    range.aspectMask = GetDefaultAspectMask(format);
    range.baseMipLevel = 0;
    range.levelCount = m_mipLevels;
    range.baseArrayLayer = 0;
    range.layerCount = m_arrayLayers;
    return CreateView(type, format, range);
}

rad::Ref<VulkanImageView> VulkanImage::CreateView(vk::ImageViewType type)
{
    return CreateView(type, m_format);
}

rad::Ref<VulkanImageView> VulkanImage::CreateView2D(uint32_t baseMipLevel,
                                                    uint32_t levelCount,
                                                    uint32_t baseArrayLayer)
{
    if (baseMipLevel >= m_mipLevels || levelCount == 0 ||
        levelCount > m_mipLevels - baseMipLevel)
    {
        throw std::out_of_range("VulkanImageView mip range is outside the image");
    }
    if (baseArrayLayer >= m_arrayLayers)
    {
        throw std::out_of_range("VulkanImageView array layer is outside the image");
    }

    vk::ImageSubresourceRange range;
    range.aspectMask = GetDefaultAspectMask(m_format);
    range.baseMipLevel = baseMipLevel;
    range.levelCount = levelCount;
    range.baseArrayLayer = baseArrayLayer;
    range.layerCount = 1;
    return CreateView(vk::ImageViewType::e2D, m_format, range);
}

VulkanImageView::VulkanImageView(rad::Ref<VulkanImage> image,
                                 const vk::ImageViewCreateInfo& createInfo) :
    m_image(std::move(image)),
    m_type(createInfo.viewType),
    m_format(createInfo.format),
    m_range(createInfo.subresourceRange),
    m_components(createInfo.components)
{
    if (!m_image)
    {
        throw std::invalid_argument("VulkanImageView requires a valid VulkanImage");
    }
    if (createInfo.image != m_image->GetHandle())
    {
        throw std::invalid_argument(
            "VulkanImageView create info must reference the supplied VulkanImage");
    }

    m_handle =
        GetDevice()->GetHandle().createImageView(createInfo, nullptr, GetDispatcher());
}

VulkanImageView::~VulkanImageView()
{
    if (m_handle)
    {
        GetDevice()->GetHandle().destroyImageView(m_handle, nullptr, GetDispatcher());
        m_handle = nullptr;
    }
}

const vk::detail::DispatchLoaderDynamic& VulkanImageView::GetDispatcher() const noexcept
{
    return m_image->GetDispatcher();
}

} // namespace sd
