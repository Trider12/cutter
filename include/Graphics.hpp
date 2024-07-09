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

enum class QueueFamily : uint8_t
{
    None = 0,
    Graphics,
    Compute,
    Transfer
};

EnumBool(Cubemap);

extern VmaAllocator allocator;
extern GpuBuffer stagingBuffer;

void initGraphics();

void terminateGraphics();

GpuBuffer createGpuBuffer(uint32_t size,
    VkBufferUsageFlags usageFlags,
    VkMemoryPropertyFlags propertyFlags,
    VmaAllocationCreateFlags createFlags);

GpuImage createGpuImage2D(VkFormat format,
    VkExtent2D extent,
    uint32_t mipLevels,
    VkImageUsageFlags usageFlags,
    VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT);

GpuImage createAndUploadGpuImage2D(const Image &image,
    QueueFamily dstQueueFamily,
    VkImageUsageFlags usageFlags,
    VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT);

GpuImage createGpuImage2DArray(VkFormat format,
    VkExtent2D extent,
    uint32_t mipLevels,
    VkImageUsageFlags usageFlags,
    Cubemap cubemap,
    VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT);

GpuImage createAndUploadGpuImage2DArray(const Image &image,
    QueueFamily dstQueueFamily,
    VkImageUsageFlags usageFlags,
    Cubemap cubemap,
    VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT);

void destroyGpuBuffer(GpuBuffer &buffer);

void destroyGpuImage(GpuImage &image);

enum class StageFlags : uint16_t
{
    None = 0,
    VertexInput = 1 << 0,
    VertexShader = 1 << 1,
    FragmentShader = 1 << 2,
    ComputeShader = 1 << 3,
    ColorAttachment = 1 << 4,
    Copy = 1 << 5,
    Blit = 1 << 6,
    Resolve = 1 << 7
};
defineEnumOperators(StageFlags, uint16_t)

enum class AccessFlags : uint8_t
{
    None = 0,
    Read = 1 << 0,
    Write = 1 << 1
};
defineEnumOperators(AccessFlags, uint8_t)

EnumBool(WaitForFence);

struct BufferBarrier
{
    VkBuffer buffer;
    StageFlags srcStageMask;
    StageFlags dstStageMask;
    AccessFlags srcAccessMask;
    AccessFlags dstAccessMask;
    QueueFamily srcQueueFamily;
    QueueFamily dstQueueFamily;
};

struct ImageBarrier
{
    VkImage image;
    StageFlags srcStageMask;
    StageFlags dstStageMask;
    AccessFlags srcAccessMask;
    AccessFlags dstAccessMask;
    VkImageLayout oldLayout;
    VkImageLayout newLayout;
    QueueFamily srcQueueFamily;
    QueueFamily dstQueueFamily;
    uint32_t baseMipLevel;
    uint32_t levelCount; // 0 == all
    uint32_t baseArrayLayer;
    uint32_t layerCount; // 0 == all
};

void pipelineBarrier(VkCommandBuffer cmd,
    BufferBarrier *bufferBarriers,
    uint32_t bufferBarrierCount,
    ImageBarrier *imageBarriers,
    uint32_t imageBarrierCount);

void beginOneTimeCmd(VkCommandBuffer cmd, VkCommandPool commandPool);

void endAndSubmitOneTimeCmd(VkCommandBuffer cmd, VkQueue queue, const VkSemaphoreSubmitInfoKHR *waitSemaphoreSubmitInfo, const VkSemaphoreSubmitInfoKHR *signalSemaphoreSubmitInfo, WaitForFence waitForFence);

void copyStagingBufferToBuffer(VkBuffer buffer, VkBufferCopy *copyRegions, uint32_t copyRegionCount, QueueFamily dstQueueFamily);

void copyStagingBufferToImage(VkImage image, VkBufferImageCopy *copyRegions, uint32_t copyRegionCount, QueueFamily dstQueueFamily, VkImageLayout dstLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

void copyImageToStagingBuffer(VkImage image, VkBufferImageCopy *copyRegions, uint32_t copyRegionCount, QueueFamily srcQueueFamily);