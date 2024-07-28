#pragma once

#include "Utils.hpp"

#include <volk/volk.h>

#define vkAssert(expr) ASSERT((expr) == VK_SUCCESS)
#define vkVerify(expr) VERIFY((expr) == VK_SUCCESS)

VkDebugUtilsLabelEXT initDebugUtilsLabelEXT(const char *label);

VkDebugUtilsObjectNameInfoEXT initDebugUtilsObjectNameInfoEXT(VkObjectType objectType, void *objectHandle, const char *objectName);

VkFenceCreateInfo initFenceCreateInfo(VkFenceCreateFlags flags = 0);

VkSemaphoreCreateInfo initSemaphoreCreateInfo();

VkCommandPoolCreateInfo initCommandPoolCreateInfo(uint32_t queueFamilyIndex, bool resetCommandBuffer);

VkCommandBufferAllocateInfo initCommandBufferAllocateInfo(VkCommandPool commandPool, uint32_t count);

VkCommandBufferBeginInfo initCommandBufferBeginInfo(VkCommandBufferUsageFlags flags = 0);

VkCommandBufferSubmitInfoKHR initCommandBufferSubmitInfo(VkCommandBuffer commandBuffer);

VkSemaphoreSubmitInfoKHR initSemaphoreSubmitInfo(VkSemaphore semaphore, VkPipelineStageFlags2KHR stageMask);

VkSubmitInfo2KHR initSubmitInfo(const VkCommandBufferSubmitInfoKHR *commandBufferSubmitInfo, const VkSemaphoreSubmitInfoKHR *waitSemaphoreSubmitInfo, const VkSemaphoreSubmitInfoKHR *signalSemaphoreSubmitInfo);

VkPresentInfoKHR initPresentInfo(const VkSwapchainKHR *swapchain, const VkSemaphore *waitSemaphore, const uint32_t *imageIndex);

VkBufferCreateInfo initBufferCreateInfo(uint32_t size, VkBufferUsageFlags usageFlags);

VkImageCreateInfo initImageCreateInfo(VkFormat format, VkExtent3D extent, uint32_t mipLevels, uint32_t arrayLayers, VkImageUsageFlags usageFlags);

VkImageSubresourceRange initImageSubresourceRange(VkImageAspectFlags aspectMask, uint32_t baseMipLevel = 0, uint32_t levelCount = 0, uint32_t baseArrayLayer = 0, uint32_t layerCount = 0);

VkImageViewCreateInfo initImageViewCreateInfo(VkImage image, VkFormat format, VkImageViewType viewType, VkImageAspectFlags aspectFlags);

VkImageViewCreateInfo initImageViewCreateInfo(VkImage image, VkFormat format, VkImageViewType viewType, VkImageSubresourceRange subresourceRange);

VkSamplerCreateInfo initSamplerCreateInfo(VkFilter filter, VkSamplerAddressMode addressMode);

VkDescriptorPoolCreateInfo initDescriptorPoolCreateInfo(uint32_t maxSets, const VkDescriptorPoolSize *poolSizes, uint32_t poolSizeCount, VkDescriptorPoolCreateFlags flags = 0);

VkDescriptorSetLayoutBindingFlagsCreateInfo initDescriptorSetLayoutBindingFlagsCreateInfo(const VkDescriptorBindingFlags *bindingFlags, uint32_t bindingCount);

VkDescriptorSetLayoutCreateInfo initDescriptorSetLayoutCreateInfo(const VkDescriptorSetLayoutBinding *bindings, uint32_t bindingCount, const VkDescriptorSetLayoutBindingFlagsCreateInfo *flagsInfo = nullptr);

VkDescriptorSetAllocateInfo initDescriptorSetAllocateInfo(VkDescriptorPool descriptorPool, const VkDescriptorSetLayout *setLayouts, uint32_t descriptorSetCount);

VkWriteDescriptorSet initWriteDescriptorSetImage(VkDescriptorSet dstSet, uint32_t dstBinding, uint32_t descriptorCount, VkDescriptorType descriptorType, const VkDescriptorImageInfo *imageInfo);

VkWriteDescriptorSet initWriteDescriptorSetBuffer(VkDescriptorSet dstSet, uint32_t dstBinding, uint32_t descriptorCount, VkDescriptorType descriptorType, const VkDescriptorBufferInfo *bufferInfo);

VkPipelineLayoutCreateInfo initPipelineLayoutCreateInfo(const VkDescriptorSetLayout *setLayouts, uint32_t setLayoutCount, const VkPushConstantRange *pushConstantRanges = nullptr, uint32_t pushConstantRangeCount = 0);

VkPipelineShaderStageCreateInfo initPipelineShaderStageCreateInfo(VkShaderStageFlagBits stage, VkShaderModule module, const VkSpecializationInfo *specializationInfo = nullptr, const char *name = "main");

VkPipelineVertexInputStateCreateInfo initPipelineVertexInputStateCreateInfo(const VkVertexInputBindingDescription *vertexBindingDescriptions,
    uint32_t vertexBindingDescriptionCount,
    const VkVertexInputAttributeDescription *vertexAttributeDescriptions,
    uint32_t vertexAttributeDescriptionCount);

VkPipelineInputAssemblyStateCreateInfo initPipelineInputAssemblyStateCreateInfo(VkPrimitiveTopology topology);

VkPipelineViewportStateCreateInfo initPipelineViewportStateCreateInfo();

VkPipelineRasterizationStateCreateInfo initPipelineRasterizationStateCreateInfo(VkPolygonMode polygonMode, VkCullModeFlags cullMode, VkFrontFace frontFace);

VkPipelineMultisampleStateCreateInfo initPipelineMultisampleStateCreateInfo();

VkPipelineDepthStencilStateCreateInfo initPipelineDepthStencilStateCreateInfo(bool depthTestEnable, bool depthWriteEnable, VkCompareOp depthCompareOp, bool stencilTestEnable = false);

VkPipelineColorBlendStateCreateInfo initPipelineColorBlendStateCreateInfo(const VkPipelineColorBlendAttachmentState *attachments, uint32_t attachmentCount);

VkPipelineDynamicStateCreateInfo initPipelineDynamicStateCreateInfo();

VkPipelineRenderingCreateInfoKHR initPipelineRenderingCreateInfo(const VkFormat *colorAttachmentFormats, uint32_t colorAttachmentCount, VkFormat depthAttachmentFormat = VK_FORMAT_UNDEFINED, VkFormat stencilAttachmentFormat = VK_FORMAT_UNDEFINED);

VkGraphicsPipelineCreateInfo initGraphicsPipelineCreateInfo(VkPipelineLayout layout,
    const VkPipelineShaderStageCreateInfo *stages,
    uint32_t stageCount,
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

VkRenderingAttachmentInfoKHR initRenderingAttachmentInfo(VkImageView imageView, VkImageLayout imageLayout, const VkClearValue *clearValue = nullptr);

VkRenderingInfoKHR initRenderingInfo(VkExtent2D renderExtent, const VkRenderingAttachmentInfo *colorAttachments, uint32_t colorAttachmentCount, const VkRenderingAttachmentInfo *depthAttachment = nullptr, const VkRenderingAttachmentInfo *stencilAttachment = nullptr);