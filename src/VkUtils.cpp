#include "VkUtils.hpp"

VkDebugUtilsLabelEXT initDebugUtilsLabelEXT(const char *label)
{
    VkDebugUtilsLabelEXT debugLabel {};
    debugLabel.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    debugLabel.pLabelName = label;

    return debugLabel;
}

VkDebugUtilsObjectNameInfoEXT initDebugUtilsObjectNameInfoEXT(VkObjectType objectType, void *objectHandle, const char *objectName)
{
    VkDebugUtilsObjectNameInfoEXT objectNameInfo {};
    objectNameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    objectNameInfo.objectType = objectType;
    objectNameInfo.objectHandle = (uint64_t)objectHandle;
    objectNameInfo.pObjectName = objectName;

    return objectNameInfo;
}

VkFenceCreateInfo initFenceCreateInfo(VkFenceCreateFlags flags)
{
    VkFenceCreateInfo fenceCreateInfo {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = flags;

    return fenceCreateInfo;
}

VkSemaphoreCreateInfo initSemaphoreCreateInfo()
{
    VkSemaphoreCreateInfo semaphoreCreateInfo {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    return semaphoreCreateInfo;
}

VkCommandPoolCreateInfo initCommandPoolCreateInfo(uint32_t queueFamilyIndex, bool resetCommandBuffer)
{
    VkCommandPoolCreateInfo commandPoolCreateInfo {};
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | (resetCommandBuffer ? VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT : 0);
    commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;

    return commandPoolCreateInfo;
}

VkCommandBufferAllocateInfo initCommandBufferAllocateInfo(VkCommandPool commandPool, uint32_t count)
{
    VkCommandBufferAllocateInfo commandBufferAllocateInfo {};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = commandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = count;

    return commandBufferAllocateInfo;
}

VkCommandBufferBeginInfo initCommandBufferBeginInfo(VkCommandBufferUsageFlags flags)
{
    VkCommandBufferBeginInfo commandBufferBeginInfo {};
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    commandBufferBeginInfo.flags = flags;

    return commandBufferBeginInfo;
}

VkCommandBufferSubmitInfoKHR initCommandBufferSubmitInfo(VkCommandBuffer commandBuffer)
{
    VkCommandBufferSubmitInfoKHR commandBufferSubmitInfo {};
    commandBufferSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR;
    commandBufferSubmitInfo.commandBuffer = commandBuffer;
    commandBufferSubmitInfo.deviceMask = 0;

    return commandBufferSubmitInfo;
}

VkSemaphoreSubmitInfoKHR initSemaphoreSubmitInfo(VkSemaphore semaphore, VkPipelineStageFlags2KHR stageMask)
{
    VkSemaphoreSubmitInfoKHR semaphoreSubmitInfo {};
    semaphoreSubmitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR;
    semaphoreSubmitInfo.semaphore = semaphore;
    semaphoreSubmitInfo.stageMask = stageMask;
    semaphoreSubmitInfo.deviceIndex = 0;

    return semaphoreSubmitInfo;
}

VkSubmitInfo2KHR initSubmitInfo(const VkCommandBufferSubmitInfoKHR *commandBufferSubmitInfo, const VkSemaphoreSubmitInfoKHR *waitSemaphoreSubmitInfos, uint32_t waitSemaphoreSubmitInfoCount, const VkSemaphoreSubmitInfoKHR *signalSemaphoreSubmitInfos, uint32_t signalSemaphoreSubmitInfoCount)
{
    VkSubmitInfo2KHR submitInfo {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR;
    submitInfo.waitSemaphoreInfoCount = waitSemaphoreSubmitInfoCount;
    submitInfo.pWaitSemaphoreInfos = waitSemaphoreSubmitInfos;
    submitInfo.signalSemaphoreInfoCount = signalSemaphoreSubmitInfoCount;
    submitInfo.pSignalSemaphoreInfos = signalSemaphoreSubmitInfos;
    submitInfo.commandBufferInfoCount = !!commandBufferSubmitInfo;
    submitInfo.pCommandBufferInfos = commandBufferSubmitInfo;

    return submitInfo;
}

VkSubmitInfo2KHR initSubmitInfo(const VkCommandBufferSubmitInfoKHR *commandBufferSubmitInfo, const VkSemaphoreSubmitInfoKHR *waitSemaphoreSubmitInfo, const VkSemaphoreSubmitInfoKHR *signalSemaphoreSubmitInfo)
{
    return initSubmitInfo(commandBufferSubmitInfo, waitSemaphoreSubmitInfo, !!waitSemaphoreSubmitInfo, signalSemaphoreSubmitInfo, !!signalSemaphoreSubmitInfo);
}

VkPresentInfoKHR initPresentInfo(const VkSwapchainKHR *swapchain, const VkSemaphore *waitSemaphore, const uint32_t *imageIndex)
{
    VkPresentInfoKHR presentInfo {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = !!waitSemaphore;
    presentInfo.pWaitSemaphores = waitSemaphore;
    presentInfo.swapchainCount = !!swapchain;
    presentInfo.pSwapchains = swapchain;
    presentInfo.pImageIndices = imageIndex;

    return presentInfo;
}

VkBufferCreateInfo initBufferCreateInfo(uint32_t size, VkBufferUsageFlags usageFlags, const uint32_t *queueFamilyIndices, uint32_t queueFamilyIndexCount)
{
    ASSERT(queueFamilyIndices && queueFamilyIndexCount || !queueFamilyIndexCount);
    VkBufferCreateInfo bufferCreateInfo {};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = size;
    bufferCreateInfo.usage = usageFlags;
    bufferCreateInfo.sharingMode = queueFamilyIndexCount ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
    bufferCreateInfo.queueFamilyIndexCount = queueFamilyIndexCount;
    bufferCreateInfo.pQueueFamilyIndices = queueFamilyIndices;

    return bufferCreateInfo;
}

VkImageCreateInfo initImageCreateInfo(VkFormat format, VkExtent3D extent, uint32_t mipLevels, uint32_t arrayLayers, VkImageUsageFlags usageFlags, const uint32_t *queueFamilyIndices, uint32_t queueFamilyIndexCount)
{
    ASSERT(queueFamilyIndices && queueFamilyIndexCount || !queueFamilyIndexCount);
    VkImageCreateInfo imageCreateInfo {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = format;
    imageCreateInfo.extent = extent;
    imageCreateInfo.mipLevels = mipLevels;
    imageCreateInfo.arrayLayers = arrayLayers;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage = usageFlags;
    imageCreateInfo.sharingMode = queueFamilyIndexCount ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.queueFamilyIndexCount = queueFamilyIndexCount;
    imageCreateInfo.pQueueFamilyIndices = queueFamilyIndices;

    return imageCreateInfo;
}

VkImageSubresourceRange initImageSubresourceRange(VkImageAspectFlags aspectMask, uint32_t baseMipLevel, uint32_t levelCount, uint32_t baseArrayLayer, uint32_t layerCount)
{
    VkImageSubresourceRange imageSubresourceRange {};
    imageSubresourceRange.aspectMask = aspectMask;
    imageSubresourceRange.baseMipLevel = baseMipLevel;
    imageSubresourceRange.levelCount = levelCount ? levelCount : VK_REMAINING_MIP_LEVELS;
    imageSubresourceRange.baseArrayLayer = baseArrayLayer;
    imageSubresourceRange.layerCount = layerCount ? layerCount : VK_REMAINING_ARRAY_LAYERS;

    return imageSubresourceRange;
}

VkImageSubresourceLayers initImageSubresourceLayers(VkImageAspectFlags aspectMask, uint32_t mipLevel, uint32_t baseArrayLayer, uint32_t layerCount)
{
    VkImageSubresourceLayers imageSubresourceLayers {};
    imageSubresourceLayers.aspectMask = aspectMask;
    imageSubresourceLayers.mipLevel = mipLevel;
    imageSubresourceLayers.baseArrayLayer = baseArrayLayer;
    imageSubresourceLayers.layerCount = layerCount ? layerCount : 1;

    return imageSubresourceLayers;
}

VkImageViewCreateInfo initImageViewCreateInfo(VkImage image, VkFormat format, VkImageViewType viewType, VkImageAspectFlags aspectFlags)
{
    return initImageViewCreateInfo(image, format, viewType, initImageSubresourceRange(aspectFlags));
}

VkImageViewCreateInfo initImageViewCreateInfo(VkImage image, VkFormat format, VkImageViewType viewType, VkImageSubresourceRange subresourceRange)
{
    VkImageViewCreateInfo imageViewCreateInfo {};
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.viewType = viewType;
    imageViewCreateInfo.image = image;
    imageViewCreateInfo.format = format;
    imageViewCreateInfo.subresourceRange = subresourceRange;

    return imageViewCreateInfo;
}

VkSamplerCreateInfo initSamplerCreateInfo(VkFilter filter, VkSamplerAddressMode addressMode)
{
    VkSamplerCreateInfo samplerCreateInfo {};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.minFilter = filter;
    samplerCreateInfo.magFilter = filter;
    samplerCreateInfo.mipmapMode = (VkSamplerMipmapMode)filter;
    samplerCreateInfo.addressModeU = addressMode;
    samplerCreateInfo.addressModeV = addressMode;
    samplerCreateInfo.addressModeW = addressMode;
    samplerCreateInfo.maxLod = 1024;

    return samplerCreateInfo;
}

VkDescriptorPoolCreateInfo initDescriptorPoolCreateInfo(uint32_t maxSets, const VkDescriptorPoolSize *poolSizes, uint32_t poolSizeCount, VkDescriptorPoolCreateFlags flags)
{
    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo {};
    descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCreateInfo.flags = flags;
    descriptorPoolCreateInfo.maxSets = maxSets;
    descriptorPoolCreateInfo.poolSizeCount = poolSizeCount;
    descriptorPoolCreateInfo.pPoolSizes = poolSizes;

    return descriptorPoolCreateInfo;
}

VkDescriptorSetLayoutBindingFlagsCreateInfo initDescriptorSetLayoutBindingFlagsCreateInfo(const VkDescriptorBindingFlags *bindingFlags, uint32_t bindingCount)
{
    VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo {};
    flagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    flagsInfo.bindingCount = bindingCount;
    flagsInfo.pBindingFlags = bindingFlags;

    return flagsInfo;
}

VkDescriptorSetLayoutCreateInfo initDescriptorSetLayoutCreateInfo(const VkDescriptorSetLayoutBinding *bindings, uint32_t bindingCount, const VkDescriptorSetLayoutBindingFlagsCreateInfo *flagsInfo)
{
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo {};
    descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.pNext = flagsInfo;
    descriptorSetLayoutCreateInfo.bindingCount = bindingCount;
    descriptorSetLayoutCreateInfo.pBindings = bindings;

    return descriptorSetLayoutCreateInfo;
}

VkDescriptorSetAllocateInfo initDescriptorSetAllocateInfo(VkDescriptorPool descriptorPool, const VkDescriptorSetLayout *setLayouts, uint32_t descriptorSetCount)
{
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo {};
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.descriptorPool = descriptorPool;
    descriptorSetAllocateInfo.descriptorSetCount = descriptorSetCount;
    descriptorSetAllocateInfo.pSetLayouts = setLayouts;

    return descriptorSetAllocateInfo;
}

static VkWriteDescriptorSet initWriteDescriptorSet(VkDescriptorSet dstSet, uint32_t dstBinding, uint32_t descriptorCount, VkDescriptorType descriptorType)
{
    VkWriteDescriptorSet writeDescriptorSet {};
    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.dstSet = dstSet;
    writeDescriptorSet.dstBinding = dstBinding;
    writeDescriptorSet.descriptorCount = descriptorCount;
    writeDescriptorSet.descriptorType = descriptorType;

    return writeDescriptorSet;
}

VkWriteDescriptorSet initWriteDescriptorSetImage(VkDescriptorSet dstSet, uint32_t dstBinding, uint32_t descriptorCount, VkDescriptorType descriptorType, const VkDescriptorImageInfo *imageInfo)
{
    VkWriteDescriptorSet writeDescriptorSet = initWriteDescriptorSet(dstSet, dstBinding, descriptorCount, descriptorType);
    writeDescriptorSet.pImageInfo = imageInfo;

    return writeDescriptorSet;
}

VkWriteDescriptorSet initWriteDescriptorSetBuffer(VkDescriptorSet dstSet, uint32_t dstBinding, uint32_t descriptorCount, VkDescriptorType descriptorType, const VkDescriptorBufferInfo *bufferInfo)
{
    VkWriteDescriptorSet writeDescriptorSet = initWriteDescriptorSet(dstSet, dstBinding, descriptorCount, descriptorType);
    writeDescriptorSet.pBufferInfo = bufferInfo;

    return writeDescriptorSet;
}

VkPipelineLayoutCreateInfo initPipelineLayoutCreateInfo(const VkDescriptorSetLayout *setLayouts, uint32_t setLayoutCount, const VkPushConstantRange *pushConstantRanges, uint32_t pushConstantRangeCount)
{
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo {};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = setLayoutCount;
    pipelineLayoutCreateInfo.pSetLayouts = setLayouts;
    pipelineLayoutCreateInfo.pushConstantRangeCount = pushConstantRangeCount;
    pipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRanges;

    return pipelineLayoutCreateInfo;
}

VkPipelineShaderStageCreateInfo initPipelineShaderStageCreateInfo(VkShaderStageFlagBits stage, VkShaderModule module, const VkSpecializationInfo *specializationInfo, const char *name)
{
    VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo {};
    pipelineShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineShaderStageCreateInfo.stage = stage;
    pipelineShaderStageCreateInfo.module = module;
    pipelineShaderStageCreateInfo.pName = name;
    pipelineShaderStageCreateInfo.pSpecializationInfo = specializationInfo;

    return pipelineShaderStageCreateInfo;
}

VkPipelineVertexInputStateCreateInfo initPipelineVertexInputStateCreateInfo(
    const VkVertexInputBindingDescription *vertexBindingDescriptions,
    uint32_t vertexBindingDescriptionCount,
    const VkVertexInputAttributeDescription *vertexAttributeDescriptions,
    uint32_t vertexAttributeDescriptionCount)
{
    VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo {};
    pipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    pipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount = vertexBindingDescriptionCount;
    pipelineVertexInputStateCreateInfo.pVertexBindingDescriptions = vertexBindingDescriptions;
    pipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount = vertexAttributeDescriptionCount;
    pipelineVertexInputStateCreateInfo.pVertexAttributeDescriptions = vertexAttributeDescriptions;

    return pipelineVertexInputStateCreateInfo;
}

VkPipelineInputAssemblyStateCreateInfo initPipelineInputAssemblyStateCreateInfo(VkPrimitiveTopology topology)
{
    VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo {};
    pipelineInputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    pipelineInputAssemblyStateCreateInfo.topology = topology;
    pipelineInputAssemblyStateCreateInfo.primitiveRestartEnable = false;

    return pipelineInputAssemblyStateCreateInfo;
}

VkPipelineViewportStateCreateInfo initPipelineViewportStateCreateInfo()
{
    VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo {};
    pipelineViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    pipelineViewportStateCreateInfo.viewportCount = 1;
    pipelineViewportStateCreateInfo.scissorCount = 1;

    return pipelineViewportStateCreateInfo;
}

VkPipelineRasterizationStateCreateInfo initPipelineRasterizationStateCreateInfo(VkPolygonMode polygonMode, VkCullModeFlags cullMode, VkFrontFace frontFace)
{
    VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo {};
    pipelineRasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    pipelineRasterizationStateCreateInfo.polygonMode = polygonMode;
    pipelineRasterizationStateCreateInfo.cullMode = cullMode;
    pipelineRasterizationStateCreateInfo.frontFace = frontFace;
    pipelineRasterizationStateCreateInfo.lineWidth = 1.f;

    return pipelineRasterizationStateCreateInfo;
}

VkPipelineMultisampleStateCreateInfo initPipelineMultisampleStateCreateInfo()
{
    VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo {};
    pipelineMultisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    pipelineMultisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    return pipelineMultisampleStateCreateInfo;
}

VkPipelineDepthStencilStateCreateInfo initPipelineDepthStencilStateCreateInfo(bool depthTestEnable, bool depthWriteEnable, VkCompareOp depthCompareOp, bool stencilTestEnable)
{
    VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo {};
    pipelineDepthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    pipelineDepthStencilStateCreateInfo.depthTestEnable = depthTestEnable;
    pipelineDepthStencilStateCreateInfo.depthWriteEnable = depthWriteEnable;
    pipelineDepthStencilStateCreateInfo.depthCompareOp = depthCompareOp;
    pipelineDepthStencilStateCreateInfo.stencilTestEnable = stencilTestEnable;

    return pipelineDepthStencilStateCreateInfo;
}

VkPipelineColorBlendStateCreateInfo initPipelineColorBlendStateCreateInfo(const VkPipelineColorBlendAttachmentState *attachments, uint32_t attachmentCount)
{
    VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo {};
    pipelineColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    pipelineColorBlendStateCreateInfo.logicOpEnable = false;
    pipelineColorBlendStateCreateInfo.attachmentCount = attachmentCount;
    pipelineColorBlendStateCreateInfo.pAttachments = attachments;

    return pipelineColorBlendStateCreateInfo;
}

VkPipelineDynamicStateCreateInfo initPipelineDynamicStateCreateInfo()
{
    static const VkDynamicState states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo {};
    pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    pipelineDynamicStateCreateInfo.dynamicStateCount = countOf(states);
    pipelineDynamicStateCreateInfo.pDynamicStates = states;

    return pipelineDynamicStateCreateInfo;
}

VkPipelineRenderingCreateInfoKHR initPipelineRenderingCreateInfo(const VkFormat *colorAttachmentFormats, uint32_t colorAttachmentCount, VkFormat depthAttachmentFormat, VkFormat stencilAttachmentFormat)
{
    VkPipelineRenderingCreateInfoKHR pipelineRenderingCreateInfo {};
    pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    pipelineRenderingCreateInfo.colorAttachmentCount = colorAttachmentCount;
    pipelineRenderingCreateInfo.pColorAttachmentFormats = colorAttachmentFormats;
    pipelineRenderingCreateInfo.depthAttachmentFormat = depthAttachmentFormat;
    pipelineRenderingCreateInfo.stencilAttachmentFormat = stencilAttachmentFormat;

    return pipelineRenderingCreateInfo;
}

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
    const VkPipelineRenderingCreateInfoKHR *renderingInfo)
{
    VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo {};
    graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    graphicsPipelineCreateInfo.pNext = renderingInfo;
    graphicsPipelineCreateInfo.stageCount = stageCount;
    graphicsPipelineCreateInfo.pStages = stages;
    graphicsPipelineCreateInfo.pVertexInputState = vertexInputState;
    graphicsPipelineCreateInfo.pInputAssemblyState = inputAssemblyState;
    graphicsPipelineCreateInfo.pTessellationState = nullptr;
    graphicsPipelineCreateInfo.pViewportState = viewportState;
    graphicsPipelineCreateInfo.pRasterizationState = rasterizationState;
    graphicsPipelineCreateInfo.pMultisampleState = multisampleState;
    graphicsPipelineCreateInfo.pDepthStencilState = depthStencilState;
    graphicsPipelineCreateInfo.pColorBlendState = colorBlendState;
    graphicsPipelineCreateInfo.pDynamicState = dynamicState;
    graphicsPipelineCreateInfo.layout = layout;
    graphicsPipelineCreateInfo.renderPass = nullptr;

    return graphicsPipelineCreateInfo;
}

VkComputePipelineCreateInfo initComputePipelineCreateInfo(VkPipelineShaderStageCreateInfo stage, VkPipelineLayout layout)
{
    VkComputePipelineCreateInfo computePipelineCreateInfo {};
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.stage = stage;
    computePipelineCreateInfo.layout = layout;

    return computePipelineCreateInfo;
}

VkRenderingAttachmentInfoKHR initRenderingAttachmentInfo(VkImageView imageView, VkImageLayout imageLayout, const VkClearValue *clearValue)
{
    VkRenderingAttachmentInfo renderingAttachmentInfo {};
    renderingAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    renderingAttachmentInfo.imageView = imageView;
    renderingAttachmentInfo.imageLayout = imageLayout;
    renderingAttachmentInfo.loadOp = clearValue ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    renderingAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    renderingAttachmentInfo.clearValue = clearValue ? *clearValue : VkClearValue();

    return renderingAttachmentInfo;
}

VkRenderingInfoKHR initRenderingInfo(VkExtent2D renderExtent, const VkRenderingAttachmentInfo *colorAttachments, uint32_t colorAttachmentCount, const VkRenderingAttachmentInfo *depthAttachment, const VkRenderingAttachmentInfo *stencilAttachment)
{
    VkRenderingInfo renderingInfo {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    renderingInfo.renderArea.extent = renderExtent;
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = colorAttachmentCount;
    renderingInfo.pColorAttachments = colorAttachments;
    renderingInfo.pDepthAttachment = depthAttachment;
    renderingInfo.pStencilAttachment = stencilAttachment;

    return renderingInfo;
}