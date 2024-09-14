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

enum class StageFlags : uint16_t
{
    None = 0,
    VertexInput = 1 << 0,
    VertexShader = 1 << 1,
    FragmentShader = 1 << 2,
    ComputeShader = 1 << 3,
    ColorAttachmentOutput = 1 << 4,
    Clear = 1 << 5,
    Copy = 1 << 6,
    Blit = 1 << 7,
    Resolve = 1 << 8,
    All = 1 << 15
};
defineEnumOperators(StageFlags, uint16_t);

enum class AccessFlags : uint8_t
{
    None = 0,
    Read = 1 << 0,
    Write = 1 << 1
};
defineEnumOperators(AccessFlags, uint8_t);

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
    VkFormat format;
    uint8_t levelCount;
    uint8_t layerCount;
    GpuImageType type;
};

void initGraphics();

void terminateGraphics();

Cmd allocateCmd(VkCommandPool pool);

Cmd allocateCmd(QueueFamily queueFamily);

void freeCmd(Cmd cmd);

GpuBuffer createGpuBuffer(uint32_t size,
    VkBufferUsageFlags usageFlags,
    VkMemoryPropertyFlags propertyFlags,
    VmaAllocationCreateFlags createFlags);

void destroyGpuBuffer(GpuBuffer &buffer);

GpuImage createGpuImage(VkFormat format,
    VkExtent2D extent,
    uint8_t mipLevels,
    VkImageUsageFlags usageFlags,
    GpuImageType type = GpuImageType::Image2D,
    VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
    uint32_t sampleCount = 1);

GpuImage createAndCopyGpuImage(const Image &image,
    QueueFamily dstQueueFamily,
    VkImageUsageFlags usageFlags,
    GpuImageType type = GpuImageType::Image2D,
    VkImageLayout dstLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
    uint8_t sampleCount = 1);

void destroyGpuImage(GpuImage &image);

void copyImage(const Image &srcImage, GpuImage &dstImage, QueueFamily dstQueueFamily, VkImageLayout dstLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

void copyImage(const GpuImage &srcImage, Image &dstImage, QueueFamily srcQueueFamily, VkImageLayout srcLayout = VK_IMAGE_LAYOUT_UNDEFINED);

void blitGpuImageMips(Cmd cmd, GpuImage &gpuImage, VkImageLayout oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    VkImageLayout newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, StageFlags srcStage = StageFlags::None, StageFlags dstStage = StageFlags::None);

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
    uint32_t imageBarrierCount,
    bool byRegion = false);

void pipelineBarrier(VkCommandBuffer cmd,
    BufferBarrier *bufferBarriers,
    uint32_t bufferBarrierCount,
    ImageBarrier *imageBarriers,
    uint32_t imageBarrierCount,
    bool byRegion = false);

EnumBool(WaitForFence);

void beginOneTimeCmd(Cmd cmd);

void endOneTimeCmd(Cmd cmd);

void endAndSubmitOneTimeCmd(Cmd cmd, VkQueue queue, const VkSemaphoreSubmitInfoKHR *waitSemaphoreSubmitInfo = nullptr, const VkSemaphoreSubmitInfoKHR *signalSemaphoreSubmitInfo = nullptr, VkFence fence = nullptr);

void endAndSubmitOneTimeCmd(Cmd cmd, VkQueue queue, const VkSemaphoreSubmitInfoKHR *waitSemaphoreSubmitInfo, const VkSemaphoreSubmitInfoKHR *signalSemaphoreSubmitInfo, WaitForFence waitForFence);

extern char *stagingBufferMappedData;

void copyStagingBufferToBuffer(GpuBuffer &buffer, VkBufferCopy *copyRegions, uint32_t copyRegionCount);

void copyBufferToStagingBuffer(GpuBuffer &buffer, VkBufferCopy *copyRegions, uint32_t copyRegionCount);

class GraphicsPipelineBuilder
{
public:
    GraphicsPipelineBuilder();

    void clear();

    void setShaders(VkShaderModule vertexShader, VkShaderModule fragmentShader); // ownsShaders = false. Fragment shader can be nullptr
    void setShaders(const char *vertexShaderSpvPath, const char *fragmentShaderSpvPath); // ownsShaders = true. Fragment shader can be nullptr
    void setPrimitiveTopology(VkPrimitiveTopology topology);
    void setPolygonMode(VkPolygonMode polygonMode);
    void setCullMode(VkCullModeFlags cullMode);
    void setFrontFace(VkFrontFace frontFace);
    void setLineWidth(float width);
    void setMsaaSampleCount(uint32_t sampleCount);
    void setDepthTest(bool enable);
    void setDepthWrite(bool enable);
    void setDepthCompareOp(VkCompareOp depthCompareOp);
    void setStencilTest(bool enable);
    void setBlendStates(VkPipelineColorBlendAttachmentState *blendStates, uint32_t blendStateCount);
    void setAttachmentFormats(const VkFormat *colorAttachmentFormats, uint32_t colorAttachmentCount);
    void setDepthFormat(VkFormat depthAttachmentFormat);
    void setStencilFormat(VkFormat stencilFormat);

    bool build(VkPipelineLayout pipelineLayout, VkPipeline &pipeline); // destroys shader modules if ownsShaders == true

private:
    VkPipelineShaderStageCreateInfo stages[2];
    VkPipelineVertexInputStateCreateInfo vertexInputState;
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState;
    VkPipelineViewportStateCreateInfo viewportState;
    VkPipelineRasterizationStateCreateInfo rasterizationState;
    VkPipelineMultisampleStateCreateInfo multisampleState;
    VkPipelineDepthStencilStateCreateInfo depthStencilState;
    VkPipelineColorBlendStateCreateInfo colorBlendState;
    VkPipelineDynamicStateCreateInfo dynamicState;
    VkPipelineRenderingCreateInfoKHR renderingInfo;
    bool ownsShaders;
};