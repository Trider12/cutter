#pragma once

#include "ImageUtils.hpp"

#define VMA_VULKAN_VERSION 1002000
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include <vk_mem_alloc.h>

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

extern VmaAllocator allocator;
extern GpuBuffer stagingBuffer;

void initGraphics();

void terminateGraphics();

GpuBuffer createGpuBuffer(uint32_t size,
    VkBufferUsageFlags usageFlags,
    VkMemoryPropertyFlags propertyFlags,
    VmaAllocationCreateFlags createFlags);

GpuBuffer createAndUploadGpuBuffer(void *data,
    uint32_t size,
    VkBufferUsageFlags usageFlags,
    VkMemoryPropertyFlags propertyFlags,
    VmaAllocationCreateFlags createFlags);

GpuImage createGpuImage2D(VkFormat format,
    VkExtent2D extent,
    VkImageUsageFlags usageFlags,
    VkImageAspectFlags aspectFlags);

GpuImage createAndUploadGpuImage2D(VkFormat format,
    VkExtent2D extent,
    VkImageUsageFlags usageFlags,
    VkImageAspectFlags aspectFlags);

GpuImage createGpuImage2DArray(VkFormat format,
    VkExtent2D extent,
    VkImageUsageFlags usageFlags,
    VkImageAspectFlags aspectFlags,
    bool cubemap);

void destroyGpuBuffer(GpuBuffer &buffer);

void destroyGpuImage(GpuImage &image);

enum class AccessFlags : uint32_t
{
    None = 0,
    Read = 1,
    Write = 2
};

inline AccessFlags operator|(AccessFlags a, AccessFlags b)
{
    return (AccessFlags)((uint32_t)a | (uint32_t)b);
}

inline AccessFlags operator&(AccessFlags a, AccessFlags b)
{
    return (AccessFlags)((uint32_t)a & (uint32_t)b);
}

enum class QueueFamily : uint8_t
{
    None = 0,
    Graphics,
    Compute,
    Transfer
};

struct BufferBarrier
{
    VkBuffer buffer;
    VkPipelineStageFlags2KHR srcStageMask;
    VkPipelineStageFlags2KHR dstStageMask;
    AccessFlags srcAccessMask;
    AccessFlags dstAccessMask;
    QueueFamily srcQueueFamily;
    QueueFamily dstQueueFamily;
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
    QueueFamily srcQueueFamily;
    QueueFamily dstQueueFamily;
};

void pipelineBarrier(VkCommandBuffer cmd,
    BufferBarrier *bufferBarriers,
    uint32_t bufferBarrierCount,
    ImageBarrier *imageBarriers,
    uint32_t imageBarrierCount);

void beginOneTimeCmd(VkCommandBuffer cmd, VkCommandPool commandPool);

void endAndSubmitOneTimeCmd(VkCommandBuffer cmd, VkQueue queue, const VkSemaphoreSubmitInfoKHR *waitSemaphoreSubmitInfo, const VkSemaphoreSubmitInfoKHR *signalSemaphoreSubmitInfo, bool wait);

void copyStagingBufferToBuffer(VkBuffer buffer, VkBufferCopy *copyRegions, uint32_t copyRegionCount, QueueFamily dstQueueFamily);

void copyStagingBufferToImage(VkImage image, VkBufferImageCopy *copyRegions, uint32_t copyRegionCount, QueueFamily dstQueueFamily);

void copyImageToStagingBuffer(VkImage image, VkBufferImageCopy *copyRegions, uint32_t copyRegionCount, QueueFamily srcQueueFamily);