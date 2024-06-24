#include "Graphics.hpp"
#include "VkUtils.hpp"

GpuBuffer createBuffer(VmaAllocator allocator,
    uint32_t size,
    VkBufferUsageFlags usageFlags,
    VkMemoryPropertyFlags propertyFlags,
    VmaAllocationCreateFlags createFlags)
{
    VkBufferCreateInfo bufferCreateInfo = initBufferCreateInfo(size, usageFlags);

    VmaAllocationCreateInfo allocationCreateInfo {};
    allocationCreateInfo.flags = createFlags;
    allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocationCreateInfo.requiredFlags = propertyFlags;

    GpuBuffer buffer;
    VmaAllocationInfo allocationInfo {};
    vkVerify(vmaCreateBuffer(allocator, &bufferCreateInfo, &allocationCreateInfo, &buffer.buffer, &buffer.allocation, &allocationInfo));

    buffer.size = size;
    buffer.mappedData = allocationInfo.pMappedData;

    return buffer;
}

GpuImage createImage2D(VmaAllocator allocator,
    VkFormat format,
    VkExtent2D extent,
    VkImageUsageFlags usageFlags,
    VkImageAspectFlags aspectFlags)
{
    VkExtent3D imageExtent = { extent.width, extent.height, 1 };
    GpuImage image;
    image.imageFormat = format;
    image.imageExtent = imageExtent;

    VkImageCreateInfo imageCreateInfo = initImageCreateInfo(format, imageExtent, 1, 1, usageFlags);
    VmaAllocationCreateInfo allocationCreateInfo {};
    allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocationCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    vkVerify(vmaCreateImage(allocator, &imageCreateInfo, &allocationCreateInfo, &image.image, &image.allocation, nullptr));

    VmaAllocatorInfo allocatorInfo {};
    vmaGetAllocatorInfo(allocator, &allocatorInfo);

    VkImageViewCreateInfo imageViewCreateInfo = initImageViewCreateInfo(image.image, format, VK_IMAGE_VIEW_TYPE_2D, aspectFlags);
    vkVerify(vkCreateImageView(allocatorInfo.device, &imageViewCreateInfo, nullptr, &image.imageView));

    return image;
}

GpuImage createImage2DArray(VmaAllocator allocator,
    VkFormat format,
    VkExtent2D extent,
    VkImageUsageFlags usageFlags,
    VkImageAspectFlags aspectFlags,
    bool cubemap)
{
    VkExtent3D imageExtent = { extent.width, extent.height, 1 };
    GpuImage image;
    image.imageFormat = format;
    image.imageExtent = imageExtent;

    VkImageCreateInfo imageCreateInfo = initImageCreateInfo(format, imageExtent, 1, 6, usageFlags);
    imageCreateInfo.flags |= cubemap ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
    VmaAllocationCreateInfo allocationCreateInfo {};
    allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocationCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    vkVerify(vmaCreateImage(allocator, &imageCreateInfo, &allocationCreateInfo, &image.image, &image.allocation, nullptr));

    VmaAllocatorInfo allocatorInfo {};
    vmaGetAllocatorInfo(allocator, &allocatorInfo);

    VkImageViewCreateInfo imageViewCreateInfo = initImageViewCreateInfo(image.image, format, cubemap ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D_ARRAY, aspectFlags);
    vkVerify(vkCreateImageView(allocatorInfo.device, &imageViewCreateInfo, nullptr, &image.imageView));

    return image;
}

void destroyBuffer(VmaAllocator allocator, GpuBuffer &buffer)
{
    vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
    buffer = {};
}

void destroyImage(VmaAllocator allocator, GpuImage &image)
{
    VmaAllocatorInfo allocatorInfo {};
    vmaGetAllocatorInfo(allocator, &allocatorInfo);
    vmaDestroyImage(allocator, image.image, image.allocation);
    vkDestroyImageView(allocatorInfo.device, image.imageView, nullptr);
    image = {};
}

static VkAccessFlagBits2KHR accessFlagsToVkAccessFlags(AccessFlags flags)
{
    return (flags & AccessFlags::Read ? VK_ACCESS_2_MEMORY_READ_BIT_KHR : VK_ACCESS_2_NONE_KHR) |
        (flags & AccessFlags::Write ? VK_ACCESS_2_MEMORY_WRITE_BIT_KHR : VK_ACCESS_2_NONE_KHR);
}

void pipelineBarrier(VkCommandBuffer cmd,
    BufferBarrier *bufferBarriers,
    uint32_t bufferBarrierCount,
    ImageBarrier *imageBarriers,
    uint32_t imageBarrierCount)
{
    VkBufferMemoryBarrier2 *bufferMemoryBarriers = bufferBarrierCount ? (VkBufferMemoryBarrier2 *)ALLOCA(bufferBarrierCount * sizeof(VkBufferMemoryBarrier2)) : nullptr;

    for (uint32_t i = 0; i < bufferBarrierCount; i++)
    {
        bufferMemoryBarriers[i].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR;
        bufferMemoryBarriers[i].pNext = nullptr;
        bufferMemoryBarriers[i].srcStageMask = bufferBarriers[i].srcStageMask;
        bufferMemoryBarriers[i].srcAccessMask = accessFlagsToVkAccessFlags(bufferBarriers[i].srcAccessMask);
        bufferMemoryBarriers[i].dstStageMask = bufferBarriers[i].dstStageMask;
        bufferMemoryBarriers[i].dstAccessMask = accessFlagsToVkAccessFlags(bufferBarriers[i].dstAccessMask);
        bufferMemoryBarriers[i].srcQueueFamilyIndex = bufferBarriers[i].srcQueueFamilyIndex;
        bufferMemoryBarriers[i].dstQueueFamilyIndex = bufferBarriers[i].dstQueueFamilyIndex;
        bufferMemoryBarriers[i].buffer = bufferBarriers[i].buffer;
        bufferMemoryBarriers[i].offset = 0;
        bufferMemoryBarriers[i].size = VK_WHOLE_SIZE;
    }

    VkImageMemoryBarrier2 *imageMemoryBarriers = imageBarrierCount ? (VkImageMemoryBarrier2 *)ALLOCA(imageBarrierCount * sizeof(VkImageMemoryBarrier2)) : nullptr;

    for (uint32_t i = 0; i < imageBarrierCount; i++)
    {
        VkImageAspectFlags aspectMask = (imageBarriers[i].newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL ||
            imageBarriers[i].newLayout == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        imageMemoryBarriers[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR;
        imageMemoryBarriers[i].pNext = nullptr;
        imageMemoryBarriers[i].srcStageMask = imageBarriers[i].srcStageMask;
        imageMemoryBarriers[i].srcAccessMask = accessFlagsToVkAccessFlags(imageBarriers[i].srcAccessMask);
        imageMemoryBarriers[i].dstStageMask = imageBarriers[i].dstStageMask;
        imageMemoryBarriers[i].dstAccessMask = accessFlagsToVkAccessFlags(imageBarriers[i].dstAccessMask);
        imageMemoryBarriers[i].oldLayout = imageBarriers[i].oldLayout;
        imageMemoryBarriers[i].newLayout = imageBarriers[i].newLayout;
        imageMemoryBarriers[i].srcQueueFamilyIndex = imageBarriers[i].srcQueueFamilyIndex;
        imageMemoryBarriers[i].dstQueueFamilyIndex = imageBarriers[i].dstQueueFamilyIndex;
        imageMemoryBarriers[i].image = imageBarriers[i].image;
        imageMemoryBarriers[i].subresourceRange = initImageSubresourceRange(aspectMask);
    }

    VkDependencyInfoKHR dependencyInfo {};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
    dependencyInfo.bufferMemoryBarrierCount = bufferBarrierCount;
    dependencyInfo.pBufferMemoryBarriers = bufferMemoryBarriers;
    dependencyInfo.imageMemoryBarrierCount = imageBarrierCount;
    dependencyInfo.pImageMemoryBarriers = imageMemoryBarriers;
    vkCmdPipelineBarrier2KHR(cmd, &dependencyInfo);
}