#define VMA_IMPLEMENTATION
#include "Graphics.hpp"
#include "VkUtils.hpp"

#include <tracy/Tracy.hpp>

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

static VkCommandPool graphicsCommandPool;
static VkCommandBuffer graphicsCmd;
static VkCommandPool computeCommandPool;
static VkCommandBuffer computeCmd;
static VkCommandPool transferCommandPool;
static VkCommandBuffer transferCmd;

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

    VkCommandPoolCreateInfo commandPoolCreateInfo = initCommandPoolCreateInfo(graphicsQueueFamilyIndex);
    VkCommandBufferAllocateInfo commandBufferAllocateInfo = initCommandBufferAllocateInfo(nullptr, 1);

    vkVerify(vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &graphicsCommandPool));
    commandBufferAllocateInfo.commandPool = graphicsCommandPool;
    vkVerify(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &graphicsCmd));

    commandPoolCreateInfo.queueFamilyIndex = computeQueueFamilyIndex;
    vkVerify(vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &computeCommandPool));
    commandBufferAllocateInfo.commandPool = computeCommandPool;
    vkVerify(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &computeCmd));

    commandPoolCreateInfo.queueFamilyIndex = transferQueueFamilyIndex;
    vkVerify(vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &transferCommandPool));
    commandBufferAllocateInfo.commandPool = transferCommandPool;
    vkVerify(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &transferCmd));
}

void terminateGraphics()
{
    vkDestroyCommandPool(device, graphicsCommandPool, nullptr);
    vkDestroyCommandPool(device, computeCommandPool, nullptr);
    vkDestroyCommandPool(device, transferCommandPool, nullptr);
    destroyGpuBuffer(stagingBuffer);
    vmaDestroyAllocator(allocator);
}

GpuBuffer createGpuBuffer(uint32_t size,
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

GpuImage createGpuImage2D(VkFormat format,
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

GpuImage createGpuImage2DArray(VkFormat format,
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

static VkAccessFlagBits2KHR accessFlagsToVkAccessFlags(AccessFlags flags)
{
    return ((bool)(flags & AccessFlags::Read) ? VK_ACCESS_2_MEMORY_READ_BIT_KHR : VK_ACCESS_2_NONE_KHR) |
        ((bool)(flags & AccessFlags::Write) ? VK_ACCESS_2_MEMORY_WRITE_BIT_KHR : VK_ACCESS_2_NONE_KHR);
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
        bufferMemoryBarriers[i].srcStageMask = bufferBarriers[i].srcStageMask;
        bufferMemoryBarriers[i].srcAccessMask = accessFlagsToVkAccessFlags(bufferBarriers[i].srcAccessMask);
        bufferMemoryBarriers[i].dstStageMask = bufferBarriers[i].dstStageMask;
        bufferMemoryBarriers[i].dstAccessMask = accessFlagsToVkAccessFlags(bufferBarriers[i].dstAccessMask);
        bufferMemoryBarriers[i].srcQueueFamilyIndex = getFamilyIndex(bufferBarriers[i].srcQueueFamily);
        bufferMemoryBarriers[i].dstQueueFamilyIndex = getFamilyIndex(bufferBarriers[i].dstQueueFamily);
        bufferMemoryBarriers[i].buffer = bufferBarriers[i].buffer;
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
        imageMemoryBarriers[i].srcStageMask = imageBarriers[i].srcStageMask;
        imageMemoryBarriers[i].srcAccessMask = accessFlagsToVkAccessFlags(imageBarriers[i].srcAccessMask);
        imageMemoryBarriers[i].dstStageMask = imageBarriers[i].dstStageMask;
        imageMemoryBarriers[i].dstAccessMask = accessFlagsToVkAccessFlags(imageBarriers[i].dstAccessMask);
        imageMemoryBarriers[i].oldLayout = imageBarriers[i].oldLayout;
        imageMemoryBarriers[i].newLayout = imageBarriers[i].newLayout;
        imageMemoryBarriers[i].srcQueueFamilyIndex = getFamilyIndex(imageBarriers[i].srcQueueFamily);
        imageMemoryBarriers[i].dstQueueFamilyIndex = getFamilyIndex(imageBarriers[i].dstQueueFamily);
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

void beginOneTimeCmd(VkCommandBuffer cmd, VkCommandPool commandPool)
{
    vkVerify(vkResetCommandPool(device, commandPool, 0));
    VkCommandBufferBeginInfo commandBufferBeginInfo = initCommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    vkVerify(vkBeginCommandBuffer(cmd, &commandBufferBeginInfo));
}

void endAndSubmitOneTimeCmd(VkCommandBuffer cmd, VkQueue queue, const VkSemaphoreSubmitInfoKHR *waitSemaphoreSubmitInfo, const VkSemaphoreSubmitInfoKHR *signalSemaphoreSubmitInfo, bool wait)
{
    VkFence fence = nullptr;

    if (wait)
    {
        VkFenceCreateInfo fenceCreateInfo = initFenceCreateInfo();
        vkVerify(vkCreateFence(device, &fenceCreateInfo, nullptr, &fence));
    }

    vkVerify(vkEndCommandBuffer(cmd));
    VkCommandBufferSubmitInfoKHR cmdSubmitInfo = initCommandBufferSubmitInfo(cmd);
    VkSubmitInfo2 submitInfo = initSubmitInfo(&cmdSubmitInfo, waitSemaphoreSubmitInfo, signalSemaphoreSubmitInfo);
    vkVerify(vkQueueSubmit2KHR(queue, 1, &submitInfo, fence));

    if (wait)
    {
        vkVerify(vkWaitForFences(device, 1, &fence, true, UINT64_MAX));
        vkDestroyFence(device, fence, nullptr);
    }
}

void copyStagingBufferToBuffer(VkBuffer buffer, VkBufferCopy *copyRegions, uint32_t copyRegionCount, QueueFamily dstQueueFamily)
{
    ZoneScoped;

    if (dstQueueFamily == QueueFamily::Transfer)
    {
        beginOneTimeCmd(transferCmd, transferCommandPool);
        vkCmdCopyBuffer(transferCmd, stagingBuffer.buffer, buffer, copyRegionCount, copyRegions);
        endAndSubmitOneTimeCmd(transferCmd, transferQueue, nullptr, nullptr, true);

        return;
    }

    VkQueue dstQueue;
    VkCommandBuffer dstCmd;
    VkCommandPool dstCmdPool;
    VkPipelineStageFlags2KHR dstStageMask;

    switch (dstQueueFamily)
    {
    case QueueFamily::Graphics:
        dstQueue = graphicsQueue;
        dstCmd = graphicsCmd;
        dstCmdPool = graphicsCommandPool;
        dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT_KHR | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR;
        break;
    case QueueFamily::Compute:
        dstQueue = computeQueue;
        dstCmd = computeCmd;
        dstCmdPool = computeCommandPool;
        dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR;
        break;
    default:
        ASSERT(false);
        return;
    }

    VkSemaphore semaphore;
    VkSemaphoreCreateInfo semaphoreCreateInfo = initSemaphoreCreateInfo();
    vkVerify(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphore));
    VkSemaphoreSubmitInfoKHR ownershipReleaseFinishedInfo = initSemaphoreSubmitInfo(semaphore, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR);

    beginOneTimeCmd(transferCmd, transferCommandPool);
    vkCmdCopyBuffer(transferCmd, stagingBuffer.buffer, buffer, copyRegionCount, copyRegions);
    BufferBarrier bufferBarrier {};
    bufferBarrier.buffer = buffer;
    bufferBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT_KHR;
    bufferBarrier.srcAccessMask = AccessFlags::Write;
    bufferBarrier.srcQueueFamily = QueueFamily::Transfer;
    bufferBarrier.dstQueueFamily = dstQueueFamily;
    pipelineBarrier(transferCmd, &bufferBarrier, 1, nullptr, 0);
    endAndSubmitOneTimeCmd(transferCmd, transferQueue, nullptr, &ownershipReleaseFinishedInfo, false);

    beginOneTimeCmd(dstCmd, dstCmdPool);
    bufferBarrier = {};
    bufferBarrier.buffer = buffer;
    bufferBarrier.dstStageMask = dstStageMask;
    bufferBarrier.dstAccessMask = AccessFlags::Read | AccessFlags::Write;
    bufferBarrier.srcQueueFamily = QueueFamily::Transfer;
    bufferBarrier.dstQueueFamily = dstQueueFamily;
    pipelineBarrier(dstCmd, &bufferBarrier, 1, nullptr, 0);
    endAndSubmitOneTimeCmd(dstCmd, dstQueue, &ownershipReleaseFinishedInfo, nullptr, true);

    vkDestroySemaphore(device, semaphore, nullptr);
}

void copyStagingBufferToImage(VkImage image, VkBufferImageCopy *copyRegions, uint32_t copyRegionCount, QueueFamily dstQueueFamily)
{
    ZoneScoped;

    if (dstQueueFamily == QueueFamily::Transfer)
    {
        beginOneTimeCmd(transferCmd, transferCommandPool);
        ImageBarrier imageBarrier {};
        imageBarrier.image = image;
        imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE_KHR;
        imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT_KHR;
        imageBarrier.srcAccessMask = AccessFlags::None;
        imageBarrier.dstAccessMask = AccessFlags::Write;
        imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        pipelineBarrier(transferCmd, nullptr, 0, &imageBarrier, 1);
        vkCmdCopyBufferToImage(transferCmd, stagingBuffer.buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, copyRegionCount, copyRegions);
        endAndSubmitOneTimeCmd(transferCmd, transferQueue, nullptr, nullptr, true);

        return;
    }

    VkQueue dstQueue;
    VkCommandBuffer dstCmd;
    VkCommandPool dstCmdPool;
    VkPipelineStageFlags2KHR dstStageMask;

    switch (dstQueueFamily)
    {
    case QueueFamily::Graphics:
        dstQueue = graphicsQueue;
        dstCmd = graphicsCmd;
        dstCmdPool = graphicsCommandPool;
        dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR;
        break;
    case QueueFamily::Compute:
        dstQueue = computeQueue;
        dstCmd = computeCmd;
        dstCmdPool = computeCommandPool;
        dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR;
        break;
    default:
        ASSERT(false);
        return;
    }

    VkSemaphore semaphore;
    VkSemaphoreCreateInfo semaphoreCreateInfo = initSemaphoreCreateInfo();
    vkVerify(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphore));
    VkSemaphoreSubmitInfoKHR ownershipReleaseFinishedInfo = initSemaphoreSubmitInfo(semaphore, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR);

    beginOneTimeCmd(transferCmd, transferCommandPool);
    ImageBarrier imageBarrier {};
    imageBarrier.image = image;
    imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE_KHR;
    imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT_KHR;
    imageBarrier.srcAccessMask = AccessFlags::None;
    imageBarrier.dstAccessMask = AccessFlags::Write;
    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    pipelineBarrier(transferCmd, nullptr, 0, &imageBarrier, 1);
    vkCmdCopyBufferToImage(transferCmd, stagingBuffer.buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, copyRegionCount, copyRegions);
    imageBarrier = {};
    imageBarrier.image = image;
    imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT_KHR;
    imageBarrier.srcAccessMask = AccessFlags::Write;
    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageBarrier.srcQueueFamily = QueueFamily::Transfer;
    imageBarrier.dstQueueFamily = dstQueueFamily;
    pipelineBarrier(transferCmd, nullptr, 0, &imageBarrier, 1);
    endAndSubmitOneTimeCmd(transferCmd, transferQueue, nullptr, &ownershipReleaseFinishedInfo, false);

    beginOneTimeCmd(dstCmd, dstCmdPool);
    imageBarrier = {};
    imageBarrier.image = image;
    imageBarrier.dstStageMask = dstStageMask;
    imageBarrier.dstAccessMask = AccessFlags::Read;
    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageBarrier.srcQueueFamily = QueueFamily::Transfer;
    imageBarrier.dstQueueFamily = dstQueueFamily;
    pipelineBarrier(dstCmd, nullptr, 0, &imageBarrier, 1);
    endAndSubmitOneTimeCmd(dstCmd, dstQueue, &ownershipReleaseFinishedInfo, nullptr, true);

    vkDestroySemaphore(device, semaphore, nullptr);
}

void copyImageToStagingBuffer(VkImage image, VkBufferImageCopy *copyRegions, uint32_t copyRegionCount, QueueFamily srcQueueFamily)
{
    ZoneScoped;

    if (srcQueueFamily == QueueFamily::Transfer)
    {
        beginOneTimeCmd(transferCmd, transferCommandPool);
        ImageBarrier imageBarrier {};
        imageBarrier.image = image;
        imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE_KHR;
        imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT_KHR;
        imageBarrier.srcAccessMask = AccessFlags::None;
        imageBarrier.dstAccessMask = AccessFlags::Read;
        imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        pipelineBarrier(transferCmd, nullptr, 0, &imageBarrier, 1);
        vkCmdCopyImageToBuffer(transferCmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer.buffer, copyRegionCount, copyRegions);
        endAndSubmitOneTimeCmd(transferCmd, transferQueue, nullptr, nullptr, true);

        return;
    }

    VkQueue srcQueue;
    VkCommandBuffer srcCmd;
    VkCommandPool srcCmdPool;
    VkPipelineStageFlags2KHR srcStageMask;

    switch (srcQueueFamily)
    {
    case QueueFamily::Graphics:
        srcQueue = graphicsQueue;
        srcCmd = graphicsCmd;
        srcCmdPool = graphicsCommandPool;
        srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR;
        break;
    case QueueFamily::Compute:
        srcQueue = computeQueue;
        srcCmd = computeCmd;
        srcCmdPool = computeCommandPool;
        srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR;
        break;
    default:
        ASSERT(false);
        return;
    }

    VkSemaphore semaphore;
    VkSemaphoreCreateInfo semaphoreCreateInfo = initSemaphoreCreateInfo();
    vkVerify(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphore));
    VkSemaphoreSubmitInfoKHR ownershipReleaseFinishedInfo = initSemaphoreSubmitInfo(semaphore, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR);

    beginOneTimeCmd(srcCmd, srcCmdPool);
    ImageBarrier imageBarrier {};
    imageBarrier.image = image;
    imageBarrier.srcStageMask = srcStageMask;
    imageBarrier.srcAccessMask = AccessFlags::Write;
    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    imageBarrier.srcQueueFamily = srcQueueFamily;
    imageBarrier.dstQueueFamily = QueueFamily::Transfer;
    pipelineBarrier(srcCmd, nullptr, 0, &imageBarrier, 1);
    endAndSubmitOneTimeCmd(srcCmd, srcQueue, nullptr, &ownershipReleaseFinishedInfo, false);

    beginOneTimeCmd(transferCmd, transferCommandPool);
    imageBarrier = {};
    imageBarrier.image = image;
    imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT_KHR;
    imageBarrier.dstAccessMask = AccessFlags::Read;
    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    imageBarrier.srcQueueFamily = srcQueueFamily;
    imageBarrier.dstQueueFamily = QueueFamily::Transfer;
    pipelineBarrier(transferCmd, nullptr, 0, &imageBarrier, 1);
    vkCmdCopyImageToBuffer(transferCmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer.buffer, copyRegionCount, copyRegions);
    endAndSubmitOneTimeCmd(transferCmd, transferQueue, &ownershipReleaseFinishedInfo, nullptr, true);

    vkDestroySemaphore(device, semaphore, nullptr);
}