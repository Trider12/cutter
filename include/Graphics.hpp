#pragma once

#include "Vma.hpp"

struct GpuBuffer
{
    VkBuffer buffer;
    VmaAllocation allocation;
    uint32_t size;
    void *mappedData;
};

struct GpuImage
{
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkExtent3D imageExtent;
    VkFormat imageFormat;
};

GpuBuffer createBuffer(VmaAllocator allocator, uint32_t size,
    VkBufferUsageFlags usageFlags,
    VkMemoryPropertyFlags propertyFlags,
    VmaAllocationCreateFlags createFlags);

GpuImage createImage2D(VmaAllocator allocator,
    VkFormat format,
    VkExtent2D extent,
    VkImageUsageFlags usageFlags,
    VkImageAspectFlags aspectFlags);

GpuImage createImage2DArray(VmaAllocator allocator,
    VkFormat format,
    VkExtent2D extent,
    VkImageUsageFlags usageFlags,
    VkImageAspectFlags aspectFlags,
    bool cubemap);

void destroyBuffer(VmaAllocator allocator, GpuBuffer &buffer);

void destroyImage(VmaAllocator allocator, GpuImage &image);

enum AccessFlags : uint32_t
{
    None = 0,
    Read = 1,
    Write = 2
};

inline AccessFlags operator|(AccessFlags a, AccessFlags b)
{
    return (AccessFlags)((uint32_t)a | (uint32_t)b);
}

struct BufferBarrier
{
    VkBuffer buffer;
    VkPipelineStageFlags2KHR srcStageMask;
    VkPipelineStageFlags2KHR dstStageMask;
    AccessFlags srcAccessMask;
    AccessFlags dstAccessMask;
    uint32_t srcQueueFamilyIndex;
    uint32_t dstQueueFamilyIndex;
};

struct ImageBarrier
{
    VkImage image;
    VkPipelineStageFlags2KHR srcStageMask;
    VkPipelineStageFlags2KHR dstStageMask;
    AccessFlags srcAccessMask;
    AccessFlags dstAccessMask;
    VkImageLayout oldLayout;
    VkImageLayout newLayout;
    uint32_t srcQueueFamilyIndex;
    uint32_t dstQueueFamilyIndex;
};

void pipelineBarrier(VkCommandBuffer cmd,
    BufferBarrier *bufferBarriers,
    uint32_t bufferBarrierCount,
    ImageBarrier *imageBarriers,
    uint32_t imageBarrierCount);