#pragma once

#include "ImageUtils.hpp"

#define VMA_VULKAN_VERSION 1002000
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include <vk_mem_alloc.h>

struct Cmd
{
    VkCommandBuffer commandBuffer;
    VkCommandPool commandPool;
};

enum class QueueFamily : uint8_t
{
    None = 0,
    Graphics,
    Compute,
    Transfer
};

struct GpuBuffer
{
    VkBuffer buffer;
    VmaAllocation allocation;
    uint32_t size;
    void *mappedData;
};

enum class GpuImageType : uint8_t
{
    Undefined = 0,
    Image2D,
    Image2DCubemap
};

struct GpuImage
{
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkExtent3D extent;
    VkFormat format : 32;
    uint8_t levelCount;
    uint8_t layerCount;
    GpuImageType type;
};

extern VmaAllocator allocator;
extern GpuBuffer stagingBuffer;

void initGraphics();

void terminateGraphics();

Cmd allocateCmd(VkCommandPool pool);

void freeCmd(Cmd cmd);

enum class SharingMode { Exclusive = 0, Concurrent = 1 };

GpuBuffer createGpuBuffer(uint32_t size,
    VkBufferUsageFlags usageFlags,
    VkMemoryPropertyFlags propertyFlags,
    VmaAllocationCreateFlags createFlags);

GpuImage createGpuImage(VkFormat format,
    VkExtent2D extent,
    uint8_t mipLevels,
    VkImageUsageFlags usageFlags,
    GpuImageType type = GpuImageType::Image2D,
    SharingMode sharingMode = SharingMode::Exclusive,
    VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT);

void copyImage(const Image &srcImage, GpuImage &dstImage, QueueFamily dstQueueFamily, VkImageLayout dstLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

void copyImage(const GpuImage &srcImage, Image &dstImage, QueueFamily srcQueueFamily);

GpuImage createAndCopyGpuImage(const Image &image,
    QueueFamily dstQueueFamily,
    VkImageUsageFlags usageFlags,
    GpuImageType type = GpuImageType::Image2D,
    SharingMode sharingMode = SharingMode::Exclusive,
    VkImageLayout dstLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
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
defineEnumOperators(StageFlags, uint16_t);

enum class AccessFlags : uint8_t
{
    None = 0,
    Read = 1 << 0,
    Write = 1 << 1
};
defineEnumOperators(AccessFlags, uint8_t);

EnumBool(WaitForFence);

struct BufferBarrier
{
    GpuBuffer buffer;
    StageFlags srcStageMask;
    StageFlags dstStageMask;
    AccessFlags srcAccessMask;
    AccessFlags dstAccessMask;
    QueueFamily srcQueueFamily;
    QueueFamily dstQueueFamily;
};

struct ImageBarrier
{
    GpuImage image;
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

void pipelineBarrier(Cmd cmd,
    BufferBarrier *bufferBarriers,
    uint32_t bufferBarrierCount,
    ImageBarrier *imageBarriers,
    uint32_t imageBarrierCount);

void pipelineBarrier(VkCommandBuffer cmd,
    BufferBarrier *bufferBarriers,
    uint32_t bufferBarrierCount,
    ImageBarrier *imageBarriers,
    uint32_t imageBarrierCount);

void beginOneTimeCmd(Cmd cmd);

void endAndSubmitOneTimeCmd(Cmd cmd, VkQueue queue, const VkSemaphoreSubmitInfoKHR *waitSemaphoreSubmitInfo, const VkSemaphoreSubmitInfoKHR *signalSemaphoreSubmitInfo, VkFence fence);

void endAndSubmitOneTimeCmd(Cmd cmd, VkQueue queue, const VkSemaphoreSubmitInfoKHR *waitSemaphoreSubmitInfo, const VkSemaphoreSubmitInfoKHR *signalSemaphoreSubmitInfo, WaitForFence waitForFence);

void copyStagingBufferToBuffer(GpuBuffer &buffer, VkBufferCopy *copyRegions, uint32_t copyRegionCount);

void copyBufferToStagingBuffer(GpuBuffer &buffer, VkBufferCopy *copyRegions, uint32_t copyRegionCount);