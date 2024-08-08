#define VMA_IMPLEMENTATION
#define VMA_LEAK_LOG_FORMAT(format, ...) do { \
       printf((format), __VA_ARGS__); \
       printf("\n"); \
   } while(false)

#include "DebugUtils.hpp"
#include "Graphics.hpp"
#include "VkUtils.hpp"

extern VkInstance instance;
extern VkPhysicalDevice physicalDevice;
extern VkDevice device;

extern VkQueue graphicsQueue;
extern uint32_t graphicsQueueFamilyIndex;
extern VkQueue computeQueue;
extern uint32_t computeQueueFamilyIndex;
extern VkQueue transferQueue;
extern uint32_t transferQueueFamilyIndex;

VmaAllocator allocator;
GpuBuffer stagingBuffer;

VkCommandPool graphicsCommandPool;
VkCommandPool computeCommandPool;
VkCommandPool transferCommandPool;

static VmaVulkanFunctions initVmaVulkanFunctions()
{
    VmaVulkanFunctions vmaVulkanFunctions = {};
    vmaVulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vmaVulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
    vmaVulkanFunctions.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
    vmaVulkanFunctions.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
    vmaVulkanFunctions.vkAllocateMemory = vkAllocateMemory;
    vmaVulkanFunctions.vkFreeMemory = vkFreeMemory;
    vmaVulkanFunctions.vkMapMemory = vkMapMemory;
    vmaVulkanFunctions.vkUnmapMemory = vkUnmapMemory;
    vmaVulkanFunctions.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
    vmaVulkanFunctions.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
    vmaVulkanFunctions.vkBindBufferMemory = vkBindBufferMemory;
    vmaVulkanFunctions.vkBindImageMemory = vkBindImageMemory;
    vmaVulkanFunctions.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
    vmaVulkanFunctions.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
    vmaVulkanFunctions.vkCreateBuffer = vkCreateBuffer;
    vmaVulkanFunctions.vkDestroyBuffer = vkDestroyBuffer;
    vmaVulkanFunctions.vkCreateImage = vkCreateImage;
    vmaVulkanFunctions.vkDestroyImage = vkDestroyImage;
    vmaVulkanFunctions.vkCmdCopyBuffer = vkCmdCopyBuffer;
    // assume Vulkan 1.2
    vmaVulkanFunctions.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2;
    vmaVulkanFunctions.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2;
    vmaVulkanFunctions.vkBindBufferMemory2KHR = vkBindBufferMemory2;
    vmaVulkanFunctions.vkBindImageMemory2KHR = vkBindImageMemory2;
    vmaVulkanFunctions.vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2;

    return vmaVulkanFunctions;
}

void initGraphics()
{
    VmaVulkanFunctions vulkanFunctions = initVmaVulkanFunctions();
    VmaAllocatorCreateInfo allocatorCreateInfo {};
    allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_2;
    allocatorCreateInfo.physicalDevice = physicalDevice;
    allocatorCreateInfo.device = device;
    allocatorCreateInfo.instance = instance;
    allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;
    vkVerify(vmaCreateAllocator(&allocatorCreateInfo, &allocator));

    stagingBuffer = createGpuBuffer(256 * 1024 * 1024,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
    setGpuBufferName(stagingBuffer, NAMEOF(stagingBuffer));

    VkCommandPoolCreateInfo commandPoolCreateInfo = initCommandPoolCreateInfo(graphicsQueueFamilyIndex, true);
    vkVerify(vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &graphicsCommandPool));

    commandPoolCreateInfo.queueFamilyIndex = computeQueueFamilyIndex;
    vkVerify(vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &computeCommandPool));

    commandPoolCreateInfo.queueFamilyIndex = transferQueueFamilyIndex;
    vkVerify(vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &transferCommandPool));
}

void terminateGraphics()
{
    vkDestroyCommandPool(device, graphicsCommandPool, nullptr);
    vkDestroyCommandPool(device, computeCommandPool, nullptr);
    vkDestroyCommandPool(device, transferCommandPool, nullptr);
    destroyGpuBuffer(stagingBuffer);
    vmaDestroyAllocator(allocator);
}

Cmd allocateCmd(VkCommandPool pool)
{
    Cmd cmd;
    VkCommandBufferAllocateInfo commandBufferAllocateInfo = initCommandBufferAllocateInfo(pool, 1);
    vkVerify(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &cmd.commandBuffer));
    cmd.commandPool = pool;
    return cmd;
}

void freeCmd(Cmd cmd)
{
    vkFreeCommandBuffers(device, cmd.commandPool, 1, &cmd.commandBuffer);
}

GpuBuffer createGpuBuffer(uint32_t size,
    VkBufferUsageFlags usageFlags,
    VkMemoryPropertyFlags propertyFlags,
    VmaAllocationCreateFlags createFlags)
{
    uint32_t queueFamilyIndices[] { graphicsQueueFamilyIndex, computeQueueFamilyIndex, transferQueueFamilyIndex };
    VkBufferCreateInfo bufferCreateInfo = initBufferCreateInfo(size, usageFlags, queueFamilyIndices, countOf(queueFamilyIndices));

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

GpuImage createGpuImage(VkFormat format, VkExtent2D extent, uint8_t mipLevels, VkImageUsageFlags usageFlags, GpuImageType type, SharingMode sharingMode, VkImageAspectFlags aspectFlags)
{
    VkExtent3D imageExtent = { extent.width, extent.height, 1 };
    GpuImage image;
    image.format = format;
    image.extent = imageExtent;
    image.levelCount = mipLevels;
    image.layerCount = 1;
    image.type = type;

    uint32_t queueFamilyIndices[] { graphicsQueueFamilyIndex, computeQueueFamilyIndex, transferQueueFamilyIndex };

    VkImageCreateInfo imageCreateInfo = initImageCreateInfo(format, imageExtent, mipLevels, 1, usageFlags,
        queueFamilyIndices, sharingMode == SharingMode::Exclusive ? 0 : countOf(queueFamilyIndices));
    VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D;

    switch (type)
    {
    case GpuImageType::Image2D:
        break;
    case GpuImageType::Image2DCubemap:
        image.layerCount = 6;
        imageCreateInfo.arrayLayers = 6;
        imageCreateInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        break;
    default:
        ASSERT(false);
        break;
    }

    VmaAllocationCreateInfo allocationCreateInfo {};
    allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocationCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    vkVerify(vmaCreateImage(allocator, &imageCreateInfo, &allocationCreateInfo, &image.image, &image.allocation, nullptr));

    VmaAllocatorInfo allocatorInfo {};
    vmaGetAllocatorInfo(allocator, &allocatorInfo);

    VkImageViewCreateInfo imageViewCreateInfo = initImageViewCreateInfo(image.image, format, viewType, aspectFlags);
    vkVerify(vkCreateImageView(allocatorInfo.device, &imageViewCreateInfo, nullptr, &image.imageView));

    return image;
}

static void copyStagingBufferToImage(GpuImage &image, VkBufferImageCopy *copyRegions, uint32_t copyRegionCount, QueueFamily dstQueueFamily, VkImageLayout dstLayout);
static void copyImageToStagingBuffer(const GpuImage &image, VkBufferImageCopy *copyRegions, uint32_t copyRegionCount, QueueFamily srcQueueFamily);

void copyImage(const Image &srcImage, GpuImage &dstImage, QueueFamily dstQueueFamily, VkImageLayout dstLayout)
{
    ASSERT(srcImage.data && srcImage.dataSize);
    ASSERT(srcImage.levelCount == dstImage.levelCount);
    ASSERT(srcImage.faceCount == dstImage.layerCount);
    ASSERT(srcImage.format == dstImage.format);
    ASSERT(srcImage.width == dstImage.extent.width);
    ASSERT(srcImage.height == dstImage.extent.height);

    uint8_t copyRegionsCount = srcImage.faceCount * srcImage.levelCount;
    VkBufferImageCopy *copyRegions = (VkBufferImageCopy *)alloca(copyRegionsCount * sizeof(VkBufferImageCopy));
    memset(copyRegions, 0, copyRegionsCount * sizeof(VkBufferImageCopy));
    memcpy(stagingBuffer.mappedData, srcImage.data, srcImage.dataSize);

    iterateImageLevelFaces(srcImage, [](const Image &image, uint8_t level, uint8_t face, uint16_t mipWidth, uint16_t mipHeight, uint32_t dataOffset, uint32_t dataSize, void *userData)
    {
        UNUSED(dataSize);
        VkBufferImageCopy *copyRegions = (VkBufferImageCopy *)userData;
        uint8_t index = level * image.faceCount + face;
        copyRegions[index].bufferOffset = dataOffset;
        copyRegions[index].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegions[index].imageSubresource.mipLevel = level;
        copyRegions[index].imageSubresource.baseArrayLayer = face;
        copyRegions[index].imageSubresource.layerCount = 1;
        copyRegions[index].imageExtent = { mipWidth, mipHeight, 1 };
    }, copyRegions);

    copyStagingBufferToImage(dstImage, copyRegions, copyRegionsCount, dstQueueFamily, dstLayout);
}

void copyImage(const GpuImage &srcImage, Image &dstImage, QueueFamily srcQueueFamily)
{
    dstImage.levelCount = srcImage.levelCount;
    dstImage.faceCount = srcImage.layerCount;
    dstImage.format = srcImage.format;
    dstImage.width = (uint16_t)srcImage.extent.width;
    dstImage.height = (uint16_t)srcImage.extent.height;

    uint8_t copyRegionsCount = dstImage.faceCount * dstImage.levelCount;
    VkBufferImageCopy *copyRegions = (VkBufferImageCopy *)alloca(copyRegionsCount * sizeof(VkBufferImageCopy));
    memset(copyRegions, 0, copyRegionsCount * sizeof(VkBufferImageCopy));

    dstImage.dataSize = iterateImageLevelFaces(dstImage, [](const Image &image, uint8_t level, uint8_t face, uint16_t mipWidth, uint16_t mipHeight, uint32_t dataOffset, uint32_t dataSize, void *userData)
    {
        UNUSED(dataSize);
        VkBufferImageCopy *copyRegions = (VkBufferImageCopy *)userData;
        uint8_t index = level * image.faceCount + face;
        copyRegions[index].bufferOffset = dataOffset;
        copyRegions[index].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegions[index].imageSubresource.mipLevel = level;
        copyRegions[index].imageSubresource.baseArrayLayer = face;
        copyRegions[index].imageSubresource.layerCount = 1;
        copyRegions[index].imageExtent = { mipWidth, mipHeight, 1 };
    }, copyRegions);

    copyImageToStagingBuffer(srcImage, copyRegions, copyRegionsCount, srcQueueFamily);

    delete[] dstImage.data;
    dstImage.data = new uint8_t[dstImage.dataSize];
    memcpy(dstImage.data, stagingBuffer.mappedData, dstImage.dataSize);
}

GpuImage createAndCopyGpuImage(const Image &image, QueueFamily dstQueueFamily, VkImageUsageFlags usageFlags, GpuImageType type, SharingMode sharingMode, VkImageLayout dstLayout, VkImageAspectFlags aspectFlags)
{
    ASSERT(image.faceCount == 1 || image.faceCount == 6);
    GpuImage gpuImage = createGpuImage(image.format, { image.width, image.height }, image.levelCount, usageFlags | VK_IMAGE_USAGE_TRANSFER_DST_BIT, type, sharingMode, aspectFlags);
    copyImage(image, gpuImage, dstQueueFamily, dstLayout);
    return gpuImage;
}

void destroyGpuBuffer(GpuBuffer &buffer)
{
    vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
    buffer = {};
}

void destroyGpuImage(GpuImage &image)
{
    VmaAllocatorInfo allocatorInfo {};
    vmaGetAllocatorInfo(allocator, &allocatorInfo);
    vmaDestroyImage(allocator, image.image, image.allocation);
    vkDestroyImageView(allocatorInfo.device, image.imageView, nullptr);
    image = {};
}

static VkAccessFlagBits2KHR getVkAccessFlags(AccessFlags flags)
{
    uint8_t value = +flags;
    return (value & AccessFlags::Read ? VK_ACCESS_2_MEMORY_READ_BIT_KHR : 0) |
        (value & AccessFlags::Write ? VK_ACCESS_2_MEMORY_WRITE_BIT_KHR : 0);
}

static VkPipelineStageFlags2KHR getVkStageFlags2(StageFlags flags)
{
    uint16_t value = +flags;
    return (value & StageFlags::VertexInput ? VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT_KHR : 0) |
        (value & StageFlags::VertexShader ? VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT_KHR : 0) |
        (value & StageFlags::FragmentShader ? VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR : 0) |
        (value & StageFlags::ComputeShader ? VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR : 0) |
        (value & StageFlags::ColorAttachment ? VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR : 0) |
        (value & StageFlags::Copy ? VK_PIPELINE_STAGE_2_COPY_BIT_KHR : 0) |
        (value & StageFlags::Blit ? VK_PIPELINE_STAGE_2_BLIT_BIT_KHR : 0) |
        (value & StageFlags::Resolve ? VK_PIPELINE_STAGE_2_RESOLVE_BIT_KHR : 0);
}

static uint32_t getFamilyIndex(QueueFamily type)
{
    switch (type)
    {
    case QueueFamily::Graphics:
        return graphicsQueueFamilyIndex;
    case QueueFamily::Compute:
        return computeQueueFamilyIndex;
    case QueueFamily::Transfer:
        return transferQueueFamilyIndex;
    default:
        return VK_QUEUE_FAMILY_IGNORED;
    }
}

void pipelineBarrier(Cmd cmd, BufferBarrier *bufferBarriers, uint32_t bufferBarrierCount, ImageBarrier *imageBarriers, uint32_t imageBarrierCount)
{
    pipelineBarrier(cmd.commandBuffer, bufferBarriers, bufferBarrierCount, imageBarriers, imageBarrierCount);
}

void pipelineBarrier(VkCommandBuffer cmd,
    BufferBarrier *bufferBarriers,
    uint32_t bufferBarrierCount,
    ImageBarrier *imageBarriers,
    uint32_t imageBarrierCount)
{
    VkBufferMemoryBarrier2 *bufferMemoryBarriers = bufferBarrierCount ? (VkBufferMemoryBarrier2 *)alloca(bufferBarrierCount * sizeof(VkBufferMemoryBarrier2)) : nullptr;

    for (uint32_t i = 0; i < bufferBarrierCount; i++)
    {
        bufferMemoryBarriers[i].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR;
        bufferMemoryBarriers[i].pNext = nullptr;
        bufferMemoryBarriers[i].srcStageMask = getVkStageFlags2(bufferBarriers[i].srcStageMask);
        bufferMemoryBarriers[i].srcAccessMask = getVkAccessFlags(bufferBarriers[i].srcAccessMask);
        bufferMemoryBarriers[i].dstStageMask = getVkStageFlags2(bufferBarriers[i].dstStageMask);
        bufferMemoryBarriers[i].dstAccessMask = getVkAccessFlags(bufferBarriers[i].dstAccessMask);
        bufferMemoryBarriers[i].srcQueueFamilyIndex = getFamilyIndex(bufferBarriers[i].srcQueueFamily);
        bufferMemoryBarriers[i].dstQueueFamilyIndex = getFamilyIndex(bufferBarriers[i].dstQueueFamily);
        bufferMemoryBarriers[i].buffer = bufferBarriers[i].buffer.buffer;
        bufferMemoryBarriers[i].offset = 0;
        bufferMemoryBarriers[i].size = VK_WHOLE_SIZE;
    }

    VkImageMemoryBarrier2 *imageMemoryBarriers = imageBarrierCount ? (VkImageMemoryBarrier2 *)alloca(imageBarrierCount * sizeof(VkImageMemoryBarrier2)) : nullptr;

    for (uint32_t i = 0; i < imageBarrierCount; i++)
    {
        VkImageAspectFlags aspectMask = (imageBarriers[i].newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL ||
            imageBarriers[i].newLayout == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        imageMemoryBarriers[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR;
        imageMemoryBarriers[i].pNext = nullptr;
        imageMemoryBarriers[i].srcStageMask = getVkStageFlags2(imageBarriers[i].srcStageMask);
        imageMemoryBarriers[i].srcAccessMask = getVkAccessFlags(imageBarriers[i].srcAccessMask);
        imageMemoryBarriers[i].dstStageMask = getVkStageFlags2(imageBarriers[i].dstStageMask);
        imageMemoryBarriers[i].dstAccessMask = getVkAccessFlags(imageBarriers[i].dstAccessMask);
        imageMemoryBarriers[i].oldLayout = imageBarriers[i].oldLayout;
        imageMemoryBarriers[i].newLayout = imageBarriers[i].newLayout;
        imageMemoryBarriers[i].srcQueueFamilyIndex = getFamilyIndex(imageBarriers[i].srcQueueFamily);
        imageMemoryBarriers[i].dstQueueFamilyIndex = getFamilyIndex(imageBarriers[i].dstQueueFamily);
        imageMemoryBarriers[i].image = imageBarriers[i].image.image;
        imageMemoryBarriers[i].subresourceRange = initImageSubresourceRange(aspectMask, imageBarriers[i].baseMipLevel, imageBarriers[i].levelCount, imageBarriers[i].baseArrayLayer, imageBarriers[i].layerCount);
    }

    VkDependencyInfoKHR dependencyInfo {};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
    dependencyInfo.bufferMemoryBarrierCount = bufferBarrierCount;
    dependencyInfo.pBufferMemoryBarriers = bufferMemoryBarriers;
    dependencyInfo.imageMemoryBarrierCount = imageBarrierCount;
    dependencyInfo.pImageMemoryBarriers = imageMemoryBarriers;
    vkCmdPipelineBarrier2KHR(cmd, &dependencyInfo);
}

void beginOneTimeCmd(Cmd cmd)
{
    VkCommandBufferBeginInfo commandBufferBeginInfo = initCommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    vkVerify(vkBeginCommandBuffer(cmd.commandBuffer, &commandBufferBeginInfo));
}

void endAndSubmitOneTimeCmd(Cmd cmd, VkQueue queue, const VkSemaphoreSubmitInfoKHR *waitSemaphoreSubmitInfo, const VkSemaphoreSubmitInfoKHR *signalSemaphoreSubmitInfo, VkFence fence)
{
    vkVerify(vkEndCommandBuffer(cmd.commandBuffer));
    VkCommandBufferSubmitInfoKHR cmdSubmitInfo = initCommandBufferSubmitInfo(cmd.commandBuffer);
    VkSubmitInfo2 submitInfo = initSubmitInfo(&cmdSubmitInfo, waitSemaphoreSubmitInfo, signalSemaphoreSubmitInfo);
    vkVerify(vkQueueSubmit2KHR(queue, 1, &submitInfo, fence));
}

void endAndSubmitOneTimeCmd(Cmd cmd, VkQueue queue, const VkSemaphoreSubmitInfoKHR *waitSemaphoreSubmitInfo, const VkSemaphoreSubmitInfoKHR *signalSemaphoreSubmitInfo, WaitForFence waitForFence)
{
    VkFence fence = nullptr;

    if (waitForFence == WaitForFence::Yes)
    {
        VkFenceCreateInfo fenceCreateInfo = initFenceCreateInfo();
        vkVerify(vkCreateFence(device, &fenceCreateInfo, nullptr, &fence));
    }

    endAndSubmitOneTimeCmd(cmd, queue, waitSemaphoreSubmitInfo, signalSemaphoreSubmitInfo, fence);

    if (waitForFence == WaitForFence::Yes)
    {
        vkVerify(vkWaitForFences(device, 1, &fence, true, UINT64_MAX));
        vkDestroyFence(device, fence, nullptr);
    }
}

void copyStagingBufferToBuffer(GpuBuffer &buffer, VkBufferCopy *copyRegions, uint32_t copyRegionCount)
{
    ZoneScoped;
    Cmd transferCmd = allocateCmd(transferCommandPool);
    beginOneTimeCmd(transferCmd);
    beginCmdLabel(transferCmd, __FUNCTION__);
    vkCmdCopyBuffer(transferCmd.commandBuffer, stagingBuffer.buffer, buffer.buffer, copyRegionCount, copyRegions);
    endCmdLabel(transferCmd);
    endAndSubmitOneTimeCmd(transferCmd, transferQueue, nullptr, nullptr, WaitForFence::Yes);
    freeCmd(transferCmd);
}

void copyBufferToStagingBuffer(GpuBuffer &buffer, VkBufferCopy *copyRegions, uint32_t copyRegionCount)
{
    ZoneScoped;
    Cmd transferCmd = allocateCmd(transferCommandPool);
    BufferBarrier bufferBarrier {}; // this barrier is needed because copyStagingBufferToImage has no fence to make vkCmdCopyBufferToImage changes visible
    bufferBarrier.buffer = stagingBuffer;
    bufferBarrier.srcStageMask = StageFlags::Copy;
    bufferBarrier.dstStageMask = StageFlags::Copy;
    bufferBarrier.srcAccessMask = AccessFlags::Read;
    bufferBarrier.dstAccessMask = AccessFlags::Write;
    beginOneTimeCmd(transferCmd);
    beginCmdLabel(transferCmd, __FUNCTION__);
    pipelineBarrier(transferCmd, &bufferBarrier, 1, nullptr, 0);
    vkCmdCopyBuffer(transferCmd.commandBuffer, buffer.buffer, stagingBuffer.buffer, copyRegionCount, copyRegions);
    endCmdLabel(transferCmd);
    endAndSubmitOneTimeCmd(transferCmd, transferQueue, nullptr, nullptr, WaitForFence::Yes);
    freeCmd(transferCmd);
}

static void copyStagingBufferToImage(GpuImage &image, VkBufferImageCopy *copyRegions, uint32_t copyRegionCount, QueueFamily dstQueueFamily, VkImageLayout dstLayout)
{
    ZoneScoped;
    Cmd transferCmd = allocateCmd(transferCommandPool);

    if (dstQueueFamily == QueueFamily::None || dstQueueFamily == QueueFamily::Transfer)
    {
        beginOneTimeCmd(transferCmd);
        beginCmdLabel(transferCmd, __FUNCTION__);
        ImageBarrier imageBarrier {};
        imageBarrier.image = image;
        imageBarrier.srcStageMask = StageFlags::None;
        imageBarrier.dstStageMask = StageFlags::Copy;
        imageBarrier.srcAccessMask = AccessFlags::None;
        imageBarrier.dstAccessMask = AccessFlags::Write;
        imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        pipelineBarrier(transferCmd, nullptr, 0, &imageBarrier, 1);
        vkCmdCopyBufferToImage(transferCmd.commandBuffer, stagingBuffer.buffer, image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, copyRegionCount, copyRegions);

        if (dstLayout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            imageBarrier = {};
            imageBarrier.image = image;
            imageBarrier.srcStageMask = StageFlags::Copy;
            imageBarrier.dstStageMask = StageFlags::None;
            imageBarrier.srcAccessMask = AccessFlags::Write;
            imageBarrier.dstAccessMask = AccessFlags::None;
            imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageBarrier.newLayout = dstLayout;
            pipelineBarrier(transferCmd, nullptr, 0, &imageBarrier, 1);
        }

        endCmdLabel(transferCmd);
        endAndSubmitOneTimeCmd(transferCmd, transferQueue, nullptr, nullptr, WaitForFence::Yes);
        freeCmd(transferCmd);
        return;
    }

    VkQueue dstQueue;
    Cmd dstCmd;
    StageFlags dstStageMask;

    switch (dstQueueFamily)
    {
    case QueueFamily::Graphics:
        dstQueue = graphicsQueue;
        dstCmd = allocateCmd(graphicsCommandPool);
        dstStageMask = StageFlags::FragmentShader;
        break;
    case QueueFamily::Compute:
        dstQueue = computeQueue;
        dstCmd = allocateCmd(computeCommandPool);
        dstStageMask = StageFlags::ComputeShader;
        break;
    default:
        ASSERT(false);
        return;
    }

    VkSemaphore semaphore;
    VkSemaphoreCreateInfo semaphoreCreateInfo = initSemaphoreCreateInfo();
    vkVerify(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphore));
    VkSemaphoreSubmitInfoKHR ownershipReleaseFinishedInfo = initSemaphoreSubmitInfo(semaphore, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR);
    ImageBarrier imageBarrier {};

    beginOneTimeCmd(transferCmd);
    beginCmdLabel(transferCmd, __FUNCTION__);
    imageBarrier.image = image;
    imageBarrier.srcStageMask = StageFlags::None;
    imageBarrier.dstStageMask = StageFlags::Copy;
    imageBarrier.srcAccessMask = AccessFlags::None;
    imageBarrier.dstAccessMask = AccessFlags::Write;
    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    pipelineBarrier(transferCmd, nullptr, 0, &imageBarrier, 1);
    vkCmdCopyBufferToImage(transferCmd.commandBuffer, stagingBuffer.buffer, image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, copyRegionCount, copyRegions);
    imageBarrier = {};
    imageBarrier.image = image;
    imageBarrier.srcStageMask = StageFlags::Copy;
    imageBarrier.dstStageMask = StageFlags::None;
    imageBarrier.srcAccessMask = AccessFlags::Write;
    imageBarrier.dstAccessMask = AccessFlags::None;
    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageBarrier.newLayout = dstLayout;
    imageBarrier.srcQueueFamily = QueueFamily::Transfer;
    imageBarrier.dstQueueFamily = dstQueueFamily;
    pipelineBarrier(transferCmd, nullptr, 0, &imageBarrier, 1);
    endCmdLabel(transferCmd);
    endAndSubmitOneTimeCmd(transferCmd, transferQueue, nullptr, &ownershipReleaseFinishedInfo, WaitForFence::No);

    beginOneTimeCmd(dstCmd);
    beginCmdLabel(dstCmd, "QFO Acquire");
    imageBarrier = {};
    imageBarrier.image = image;
    imageBarrier.dstStageMask = dstStageMask;
    imageBarrier.dstAccessMask = AccessFlags::Read | AccessFlags::Write;
    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageBarrier.newLayout = dstLayout;
    imageBarrier.srcQueueFamily = QueueFamily::Transfer;
    imageBarrier.dstQueueFamily = dstQueueFamily;
    pipelineBarrier(dstCmd, nullptr, 0, &imageBarrier, 1);
    endCmdLabel(dstCmd);
    endAndSubmitOneTimeCmd(dstCmd, dstQueue, &ownershipReleaseFinishedInfo, nullptr, WaitForFence::Yes);

    vkDestroySemaphore(device, semaphore, nullptr);
    freeCmd(transferCmd);
    freeCmd(dstCmd);
}

static void copyImageToStagingBuffer(const GpuImage &image, VkBufferImageCopy *copyRegions, uint32_t copyRegionCount, QueueFamily srcQueueFamily)
{
    ZoneScoped;
    Cmd transferCmd = allocateCmd(transferCommandPool);
    BufferBarrier bufferBarrier {}; // this barrier is needed because copyStagingBufferToImage has no fence to make vkCmdCopyBufferToImage changes visible
    bufferBarrier.buffer = stagingBuffer;
    bufferBarrier.srcStageMask = StageFlags::Copy;
    bufferBarrier.dstStageMask = StageFlags::Copy;
    bufferBarrier.srcAccessMask = AccessFlags::Read;
    bufferBarrier.dstAccessMask = AccessFlags::Write;

    if (srcQueueFamily == QueueFamily::Transfer)
    {
        beginCmdLabel(transferCmd, __FUNCTION__);
        ImageBarrier imageBarrier {};
        imageBarrier.image = image;
        imageBarrier.srcStageMask = StageFlags::None;
        imageBarrier.dstStageMask = StageFlags::Copy;
        imageBarrier.srcAccessMask = AccessFlags::None;
        imageBarrier.dstAccessMask = AccessFlags::Read;
        imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        pipelineBarrier(transferCmd, &bufferBarrier, 1, &imageBarrier, 1);
        vkCmdCopyImageToBuffer(transferCmd.commandBuffer, image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer.buffer, copyRegionCount, copyRegions);
        endCmdLabel(transferCmd);
        endAndSubmitOneTimeCmd(transferCmd, transferQueue, nullptr, nullptr, WaitForFence::Yes);
        freeCmd(transferCmd);
        return;
    }

    VkQueue srcQueue;
    Cmd srcCmd;
    StageFlags srcStageMask;

    switch (srcQueueFamily)
    {
    case QueueFamily::Graphics:
        srcQueue = graphicsQueue;
        srcCmd = allocateCmd(graphicsCommandPool);
        srcStageMask = StageFlags::FragmentShader;
        break;
    case QueueFamily::Compute:
        srcQueue = computeQueue;
        srcCmd = allocateCmd(computeCommandPool);;
        srcStageMask = StageFlags::ComputeShader;
        break;
    default:
        ASSERT(false);
        return;
    }

    VkSemaphore semaphore;
    VkSemaphoreCreateInfo semaphoreCreateInfo = initSemaphoreCreateInfo();
    vkVerify(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphore));
    VkSemaphoreSubmitInfoKHR ownershipReleaseFinishedInfo = initSemaphoreSubmitInfo(semaphore, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR);
    ImageBarrier imageBarrier {};

    beginOneTimeCmd(srcCmd);
    beginCmdLabel(srcCmd, "QFO Release");
    imageBarrier.image = image;
    imageBarrier.srcStageMask = srcStageMask;
    imageBarrier.srcAccessMask = AccessFlags::Read | AccessFlags::Write;
    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    imageBarrier.srcQueueFamily = srcQueueFamily;
    imageBarrier.dstQueueFamily = QueueFamily::Transfer;
    pipelineBarrier(srcCmd, nullptr, 0, &imageBarrier, 1);
    endCmdLabel(srcCmd);
    endAndSubmitOneTimeCmd(srcCmd, srcQueue, nullptr, &ownershipReleaseFinishedInfo, WaitForFence::No);

    beginOneTimeCmd(transferCmd);
    beginCmdLabel(transferCmd, __FUNCTION__);
    imageBarrier = {};
    imageBarrier.image = image;
    imageBarrier.dstStageMask = StageFlags::Copy;
    imageBarrier.dstAccessMask = AccessFlags::Read;
    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    imageBarrier.srcQueueFamily = srcQueueFamily;
    imageBarrier.dstQueueFamily = QueueFamily::Transfer;
    pipelineBarrier(transferCmd, &bufferBarrier, 1, &imageBarrier, 1);
    vkCmdCopyImageToBuffer(transferCmd.commandBuffer, image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer.buffer, copyRegionCount, copyRegions);
    endCmdLabel(transferCmd);
    endAndSubmitOneTimeCmd(transferCmd, transferQueue, &ownershipReleaseFinishedInfo, nullptr, WaitForFence::Yes);

    vkDestroySemaphore(device, semaphore, nullptr);
    freeCmd(transferCmd);
    freeCmd(srcCmd);
}