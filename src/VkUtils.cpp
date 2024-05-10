#include "VkUtils.hpp"

VkFenceCreateInfo initFenceCreateInfo(VkFenceCreateFlags flags)
{
    VkFenceCreateInfo fenceCreateInfo {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = flags;

    return fenceCreateInfo;
}

VkSemaphoreCreateInfo initSemaphoreCreateInfo(VkSemaphoreCreateFlags flags)
{
    VkSemaphoreCreateInfo semaphoreCreateInfo {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.flags = flags;

    return semaphoreCreateInfo;
}

VkCommandPoolCreateInfo initCommandPoolCreateInfo(uint32_t queueFamilyIndex)
{
    VkCommandPoolCreateInfo commandPoolCreateInfo {};
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
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

VkSemaphoreSubmitInfoKHR initSemaphoreSubmitInfo(VkSemaphore semaphore, VkPipelineStageFlags2KHR stageMask)
{
    VkSemaphoreSubmitInfoKHR semaphoreSubmitInfo {};
    semaphoreSubmitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR;
    semaphoreSubmitInfo.semaphore = semaphore;
    semaphoreSubmitInfo.stageMask = stageMask;
    semaphoreSubmitInfo.deviceIndex = 0;

    return semaphoreSubmitInfo;
}

VkCommandBufferSubmitInfoKHR initCommandBufferSubmitInfo(VkCommandBuffer commandBuffer)
{
    VkCommandBufferSubmitInfoKHR commandBufferSubmitInfo {};
    commandBufferSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR;
    commandBufferSubmitInfo.commandBuffer = commandBuffer;
    commandBufferSubmitInfo.deviceMask = 0;

    return commandBufferSubmitInfo;
}

VkSubmitInfo2KHR initSubmitInfo(const VkCommandBufferSubmitInfoKHR *commandBufferSubmitInfo, const VkSemaphoreSubmitInfoKHR *waitSemaphoreSubmitInfo, const VkSemaphoreSubmitInfoKHR *signalSemaphoreSubmitInfo)
{
    VkSubmitInfo2KHR submitInfo {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR;
    submitInfo.waitSemaphoreInfoCount = !!waitSemaphoreSubmitInfo;
    submitInfo.pWaitSemaphoreInfos = waitSemaphoreSubmitInfo;
    submitInfo.signalSemaphoreInfoCount = !!signalSemaphoreSubmitInfo;
    submitInfo.pSignalSemaphoreInfos = signalSemaphoreSubmitInfo;
    submitInfo.commandBufferInfoCount = !!commandBufferSubmitInfo;
    submitInfo.pCommandBufferInfos = commandBufferSubmitInfo;

    return submitInfo;
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

VkBufferCreateInfo initBufferCreateInfo(size_t size, VkBufferUsageFlags usageFlags)
{
    VkBufferCreateInfo bufferCreateInfo {};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = size;
    bufferCreateInfo.usage = usageFlags;

    return bufferCreateInfo;
}

VkImageCreateInfo initImageCreateInfo(VkFormat format, VkExtent3D extent, VkImageUsageFlags usageFlags, VkImageLayout layout)
{
    VkImageCreateInfo imageCreateInfo {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = format;
    imageCreateInfo.extent = extent;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage = usageFlags;
    imageCreateInfo.initialLayout = layout;

    return imageCreateInfo;
}

VkImageViewCreateInfo initImageViewCreateInfo(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{
    VkImageViewCreateInfo imageViewCreateInfo {};
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.image = image;
    imageViewCreateInfo.format = format;
    imageViewCreateInfo.subresourceRange.aspectMask = aspectFlags;
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.subresourceRange.levelCount = 1;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount = 1;

    return imageViewCreateInfo;
}

VkImageSubresourceRange initImageSubresourceRange(VkImageAspectFlags aspectMask)
{
    VkImageSubresourceRange imageSubresourceRange {};
    imageSubresourceRange.aspectMask = aspectMask;
    imageSubresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    imageSubresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    return imageSubresourceRange;
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

VkDescriptorSetLayoutCreateInfo initDescriptorSetLayoutCreateInfo(const VkDescriptorSetLayoutBinding *bindings, uint32_t bindingCount)
{
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo {};
    descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
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

VkPipelineLayoutCreateInfo initPipelineLayoutCreateInfo(const VkDescriptorSetLayout *setLayouts, uint32_t setLayoutCount)
{
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo {};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = setLayoutCount;
    pipelineLayoutCreateInfo.pSetLayouts = setLayouts;

    return pipelineLayoutCreateInfo;
}

VkPipelineShaderStageCreateInfo initPipelineShaderStageCreateInfo(VkShaderStageFlagBits stage, VkShaderModule module, const char *name)
{
    VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo {};
    pipelineShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineShaderStageCreateInfo.stage = stage;
    pipelineShaderStageCreateInfo.module = module;
    pipelineShaderStageCreateInfo.pName = name;

    return pipelineShaderStageCreateInfo;
}

VkPipelineVertexInputStateCreateInfo initPipelineVertexInputStateCreateInfo(
    uint32_t vertexBindingDescriptionCount,
    const VkVertexInputBindingDescription *vertexBindingDescriptions,
    uint32_t vertexAttributeDescriptionCount,
    const VkVertexInputAttributeDescription *vertexAttributeDescriptions)
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

VkPipelineDepthStencilStateCreateInfo initPipelineDepthStencilStateCreateInfo(bool depthTestEnable, bool depthWriteEnable, bool stencilTestEnable)
{
    VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo {};
    pipelineDepthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    pipelineDepthStencilStateCreateInfo.depthBoundsTestEnable = depthTestEnable;
    pipelineDepthStencilStateCreateInfo.depthWriteEnable = depthWriteEnable;
    pipelineDepthStencilStateCreateInfo.stencilTestEnable = stencilTestEnable;
    pipelineDepthStencilStateCreateInfo.minDepthBounds = 0.f;
    pipelineDepthStencilStateCreateInfo.maxDepthBounds = 1.f;

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
    static const VkDynamicState states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo {};
    pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    pipelineDynamicStateCreateInfo.dynamicStateCount = COUNTOF(states);
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

VkRenderingAttachmentInfoKHR initRenderingAttachmentInfo(VkImageView imageView, VkClearValue *clearValue, VkImageLayout imageLayout)
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

AllocatedBuffer createBuffer(VmaAllocator allocator,
    size_t size,
    VkBufferUsageFlags usageFlags,
    VmaAllocationCreateFlags createFlags,
    VkMemoryPropertyFlags propertyFlags)
{
    VkBufferCreateInfo bufferCreateInfo = initBufferCreateInfo(size, usageFlags);

    VmaAllocationCreateInfo allocationCreateInfo {};
    allocationCreateInfo.flags = createFlags;
    allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocationCreateInfo.requiredFlags = propertyFlags;

    AllocatedBuffer buffer;
    vkVerify(vmaCreateBuffer(allocator, &bufferCreateInfo, &allocationCreateInfo, &buffer.buffer, &buffer.allocation, nullptr));

    return buffer;
}

AllocatedImage createImage(VkDevice device,
    VmaAllocator allocator,
    VkFormat format,
    VkExtent3D extent,
    VkImageLayout layout,
    VkImageAspectFlags aspectFlags,
    VkImageUsageFlags usageFlags,
    VmaAllocationCreateFlags createFlags,
    VkMemoryPropertyFlags propertyFlags)
{
    VkImageCreateInfo imageCreateInfo = initImageCreateInfo(format, extent, usageFlags, layout);

    VmaAllocationCreateInfo allocationCreateInfo {};
    allocationCreateInfo.flags = createFlags;
    allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocationCreateInfo.requiredFlags = propertyFlags;

    AllocatedImage image;
    vkVerify(vmaCreateImage(allocator, &imageCreateInfo, &allocationCreateInfo, &image.image, &image.allocation, nullptr));

    VkImageViewCreateInfo imageViewCreateInfo = initImageViewCreateInfo(image.image, format, aspectFlags);
    vkVerify(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &image.imageView));

    image.imageFormat = format;
    image.imageExtent = extent;

    return image;
}

void transitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout, VkPipelineStageFlags2KHR srcStageMask, VkPipelineStageFlags2KHR dstStageMask)
{
    VkImageAspectFlags aspectMask = (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

    VkImageMemoryBarrier2KHR imageMemoryBarrier {};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR;
    imageMemoryBarrier.srcStageMask = srcStageMask;
    imageMemoryBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT_KHR;
    imageMemoryBarrier.dstStageMask = dstStageMask;
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT_KHR | VK_ACCESS_2_MEMORY_READ_BIT_KHR;
    imageMemoryBarrier.oldLayout = currentLayout;
    imageMemoryBarrier.newLayout = newLayout;
    imageMemoryBarrier.image = image;
    imageMemoryBarrier.subresourceRange = initImageSubresourceRange(aspectMask);

    VkDependencyInfoKHR dependencyInfo {};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers = &imageMemoryBarrier;

    vkCmdPipelineBarrier2KHR(cmd, &dependencyInfo);
}

void copyImage(VkCommandBuffer cmd, VkImage src, VkImage dest, VkExtent2D srcSize, VkExtent2D dstSize)
{
    VkImageBlit blit {};
    blit.srcOffsets[1].x = srcSize.width;
    blit.srcOffsets[1].y = srcSize.height;
    blit.srcOffsets[1].z = 1;
    blit.dstOffsets[1].x = dstSize.width;
    blit.dstOffsets[1].y = dstSize.height;
    blit.dstOffsets[1].z = 1;
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.mipLevel = 0;
    blit.srcSubresource.baseArrayLayer = 0;
    blit.srcSubresource.layerCount = 1;
    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.mipLevel = 0;
    blit.dstSubresource.baseArrayLayer = 0;
    blit.dstSubresource.layerCount = 1;

    vkCmdBlitImage(cmd, src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dest, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
}

static const char *baseShaderPath = nullptr;
static shaderc_compiler_t compiler = nullptr;
static shaderc_compile_options_t options = nullptr;

static shaderc_include_result *shadercIncludeResolve(void *userData,
    const char *requestedSource,
    int type,
    const char *requestingSource,
    size_t includeDepth)
{
    (void)userData;
    (void)type;
    (void)requestingSource;
    (void)includeDepth;

    // caching might be a good idea
    char *filepath = new char[256];
    snprintf(filepath, 256, "%s/%s", baseShaderPath, requestedSource);
    uint32_t fileSize = readFile(filepath, nullptr, 0);
    char *fileData = new char[fileSize];
    readFile(filepath, fileData, fileSize);

    shaderc_include_result *result = new shaderc_include_result();
    result->source_name = filepath;
    result->source_name_length = strlen(filepath);
    result->content = fileData;
    result->content_length = fileSize;

    return result;
}

static void shadercIncludeResultRelease(void *userData, shaderc_include_result *result)
{
    (void)userData;

    delete[] result->source_name;
    delete[] result->content;
    delete result;
}

void initShaderCompiler(const char *baseShaderDirPath)
{
    baseShaderPath = baseShaderDirPath;
    compiler = shaderc_compiler_initialize();
    options = shaderc_compile_options_initialize();
    shaderc_compile_options_set_optimization_level(options, shaderc_optimization_level_performance);
    shaderc_compile_options_set_include_callbacks(options, shadercIncludeResolve, shadercIncludeResultRelease, nullptr);
}

void terminateShaderCompiler()
{
    shaderc_compiler_release(compiler);
    shaderc_compile_options_release(options);
}

void compileShaderIntoSpv(const char *shaderFilename, const char *spvFilename, shaderc_shader_kind kind)
{
    uint32_t fileSize = readFile(shaderFilename, nullptr, 0);
    char *fileData = new char[fileSize];
    readFile(shaderFilename, fileData, fileSize);
    shaderc_compilation_result_t result = shaderc_compile_into_spv(compiler, fileData, fileSize, kind, shaderFilename, "main", options);
    shaderc_compilation_status status = shaderc_result_get_compilation_status(result);

    if (status == shaderc_compilation_status_compilation_error)
    {
        fputs(shaderc_result_get_error_message(result), stderr);
    }

    ASSERT(status == shaderc_compilation_status_success);
    writeFile(spvFilename, shaderc_result_get_bytes(result), (uint32_t)shaderc_result_get_length(result));
    shaderc_result_release(result);
    delete[] fileData;
}

VkShaderModule createShaderModuleFromSpv(VkDevice device, const char *spvFilename)
{
    uint32_t fileSize = readFile(spvFilename, nullptr, 0);
    char *fileData = new char[fileSize];
    readFile(spvFilename, fileData, fileSize);

    VkShaderModule shaderModule;
    VkShaderModuleCreateInfo createInfo {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = fileSize;
    createInfo.pCode = (uint32_t *)fileData;
    vkVerify(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule));
    delete[] fileData;

    return shaderModule;
}