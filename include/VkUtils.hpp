#pragma once

#include <Utils.hpp>
#include <Vma.hpp>

#include <shaderc/shaderc.h>
#include <volk/volk.h>

#define vkAssert(expr) ASSERT((expr) == VK_SUCCESS)
#define vkVerify(expr) VERIFY((expr) == VK_SUCCESS)

// Inits

VkFenceCreateInfo initFenceCreateInfo(VkFenceCreateFlags flags = 0);

VkSemaphoreCreateInfo initSemaphoreCreateInfo(VkSemaphoreCreateFlags flags = 0);

VkCommandPoolCreateInfo initCommandPoolCreateInfo(uint32_t queueFamilyIndex);

VkCommandBufferAllocateInfo initCommandBufferAllocateInfo(VkCommandPool commandPool, uint32_t count);

VkCommandBufferBeginInfo initCommandBufferBeginInfo(VkCommandBufferUsageFlags flags = 0);

VkSemaphoreSubmitInfoKHR initSemaphoreSubmitInfo(VkSemaphore semaphore, VkPipelineStageFlags2KHR stageMask);

VkCommandBufferSubmitInfoKHR initCommandBufferSubmitInfo(VkCommandBuffer commandBuffer);

VkSubmitInfo2KHR initSubmitInfo(const VkCommandBufferSubmitInfoKHR *commandBufferSubmitInfo, const VkSemaphoreSubmitInfoKHR *waitSemaphoreSubmitInfo, const VkSemaphoreSubmitInfoKHR *signalSemaphoreSubmitInfo);

VkPresentInfoKHR initPresentInfo(const VkSwapchainKHR *swapchain, const VkSemaphore *waitSemaphore, const uint32_t *imageIndex);

VkBufferCreateInfo initBufferCreateInfo(size_t size, VkBufferUsageFlags usageFlags);

VkImageCreateInfo initImageCreateInfo(VkFormat format, VkExtent3D extent, VkImageUsageFlags usageFlags, VkImageLayout layout);

VkImageViewCreateInfo initImageViewCreateInfo(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);

VkImageSubresourceRange initImageSubresourceRange(VkImageAspectFlags aspectMask);

VkDescriptorPoolCreateInfo initDescriptorPoolCreateInfo(uint32_t maxSets, const VkDescriptorPoolSize *poolSizes, uint32_t poolSizeCount, VkDescriptorPoolCreateFlags flags = 0);

VkDescriptorSetLayoutCreateInfo initDescriptorSetLayoutCreateInfo(const VkDescriptorSetLayoutBinding *bindings, uint32_t bindingCount);

VkDescriptorSetAllocateInfo initDescriptorSetAllocateInfo(VkDescriptorPool descriptorPool, const VkDescriptorSetLayout *setLayouts, uint32_t descriptorSetCount);

VkWriteDescriptorSet initWriteDescriptorSetImage(VkDescriptorSet dstSet, uint32_t dstBinding, uint32_t descriptorCount, VkDescriptorType descriptorType, const VkDescriptorImageInfo *imageInfo);

VkWriteDescriptorSet initWriteDescriptorSetBuffer(VkDescriptorSet dstSet, uint32_t dstBinding, uint32_t descriptorCount, VkDescriptorType descriptorType, const VkDescriptorBufferInfo *bufferInfo);

VkPipelineLayoutCreateInfo initPipelineLayoutCreateInfo(const VkDescriptorSetLayout *setLayouts, uint32_t setLayoutCount);

VkPipelineShaderStageCreateInfo initPipelineShaderStageCreateInfo(VkShaderStageFlagBits stage, VkShaderModule module, const char *name = "main");

VkPipelineVertexInputStateCreateInfo initPipelineVertexInputStateCreateInfo(uint32_t vertexBindingDescriptionCount,
    const VkVertexInputBindingDescription *vertexBindingDescriptions,
    uint32_t vertexAttributeDescriptionCount,
    const VkVertexInputAttributeDescription *vertexAttributeDescriptions);

VkPipelineInputAssemblyStateCreateInfo initPipelineInputAssemblyStateCreateInfo(VkPrimitiveTopology topology);

VkPipelineViewportStateCreateInfo initPipelineViewportStateCreateInfo();

VkPipelineRasterizationStateCreateInfo initPipelineRasterizationStateCreateInfo(VkPolygonMode polygonMode, VkCullModeFlags cullMode, VkFrontFace frontFace);

VkPipelineMultisampleStateCreateInfo initPipelineMultisampleStateCreateInfo();

VkPipelineDepthStencilStateCreateInfo initPipelineDepthStencilStateCreateInfo(bool depthTestEnable, bool depthWriteEnable, bool stencilTestEnable = false);

VkPipelineColorBlendStateCreateInfo initPipelineColorBlendStateCreateInfo(const VkPipelineColorBlendAttachmentState *attachments, uint32_t attachmentCount);

VkPipelineDynamicStateCreateInfo initPipelineDynamicStateCreateInfo();

VkPipelineRenderingCreateInfoKHR initPipelineRenderingCreateInfo(const VkFormat *colorAttachmentFormats, uint32_t colorAttachmentCount, VkFormat depthAttachmentFormat = VK_FORMAT_UNDEFINED, VkFormat stencilAttachmentFormat = VK_FORMAT_UNDEFINED);

VkGraphicsPipelineCreateInfo initGraphicsPipelineCreateInfo(VkPipelineLayout layout,
    uint32_t stageCount,
    const VkPipelineShaderStageCreateInfo *stages,
    const VkPipelineVertexInputStateCreateInfo *vertexInputState,
    const VkPipelineInputAssemblyStateCreateInfo *inputAssemblyState,
    const VkPipelineViewportStateCreateInfo *viewportState,
    const VkPipelineRasterizationStateCreateInfo *rasterizationState,
    const VkPipelineMultisampleStateCreateInfo *multisampleState,
    const VkPipelineDepthStencilStateCreateInfo *depthStencilState,
    const VkPipelineColorBlendStateCreateInfo *colorBlendState,
    const VkPipelineDynamicStateCreateInfo *dynamicState,
    const VkPipelineRenderingCreateInfoKHR *renderingInfo);

VkComputePipelineCreateInfo initComputePipelineCreateInfo(VkPipelineShaderStageCreateInfo stage, VkPipelineLayout layout);

VkRenderingAttachmentInfoKHR initRenderingAttachmentInfo(VkImageView imageView, VkClearValue *clearValue = nullptr, VkImageLayout imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

VkRenderingInfoKHR initRenderingInfo(VkExtent2D renderExtent, const VkRenderingAttachmentInfo *colorAttachments, uint32_t colorAttachmentCount, const VkRenderingAttachmentInfo *depthAttachment = nullptr, const VkRenderingAttachmentInfo *stencilAttachment = nullptr);

// Utils

struct AllocatedBuffer
{
    VkBuffer buffer;
    VmaAllocation allocation;
};

struct AllocatedImage
{
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkExtent3D imageExtent;
    VkFormat imageFormat;
};

AllocatedBuffer createBuffer(VmaAllocator allocator,
    size_t size,
    VkBufferUsageFlags usageFlags,
    VmaAllocationCreateFlags createFlags,
    VkMemoryPropertyFlags propertyFlags);

AllocatedImage createImage(VkDevice device,
    VmaAllocator allocator,
    VkFormat format,
    VkExtent3D extent,
    VkImageLayout layout,
    VkImageAspectFlags aspectFlags,
    VkImageUsageFlags usageFlags,
    VmaAllocationCreateFlags createFlags,
    VkMemoryPropertyFlags propertyFlags);

void transitionImage(VkCommandBuffer cmd,
    VkImage image,
    VkImageLayout currentLayout,
    VkImageLayout newLayout,
    VkPipelineStageFlags2KHR srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
    VkPipelineStageFlags2KHR dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR);

void copyImage(VkCommandBuffer cmd,
    VkImage src,
    VkImage dest,
    VkExtent2D srcSize,
    VkExtent2D dstSize);

// Shaders

void initShaderCompiler(const char *baseShaderDirPath);

void terminateShaderCompiler();

void compileShaderIntoSpv(const char *shaderFilename, const char *spvFilename, shaderc_shader_kind kind);

VkShaderModule createShaderModuleFromSpv(VkDevice device, const char *spvFilename);