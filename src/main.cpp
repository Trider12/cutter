#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <VkBootstrap.h>

#include "Glm.hpp"
#include "Scene.hpp"
#include "VkUtils.hpp"

GLFWwindow *window;

void init();
void update();
void draw();
void exit();

int main()
{
    init();

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        update();
        draw();
    }

    exit();
    return 0;
}

const char *const appName = "Cutter";
const char *const assetsPath = "assets";

const char *const baseShaderDirPath = "assets/shaders";
const char *const vertexShaderPath = "assets/shaders/basic.vert";
const char *const fragmentShaderPath = "assets/shaders/basic.frag";
const char *const computeShaderPath = "assets/shaders/compute.comp";

const char *const vertexShaderSpvPath = "assets/spv/basic.vert.spv";
const char *const fragmentShaderSpvPath = "assets/spv/basic.frag.spv";
const char *const computeShaderSpvPath = "assets/spv/compute.comp.spv";

const char *const lanternGlbPath = "assets/glb/Lantern.glb";
const char *const lanternSceneDataPath = "assets/meshes/Lantern.bin";

const uint8_t maxFramesInFlight = 2;
uint8_t frameIndex = 0;
VkExtent2D windowExtent {1280, 720};

VkInstance instance;
VkDebugUtilsMessengerEXT debugMessenger;
VkPhysicalDevice physicalDevice;
VkPhysicalDeviceProperties physicalDeviceProperties;
VkDevice device;
VkSurfaceKHR surface;
VmaAllocator allocator;

VkQueue graphicsQueue;
uint32_t graphicsQueueFamily;

VkSwapchainKHR swapchain;
VkFormat swapchainImageFormat;
VkExtent2D swapchainImageExtent;
std::vector<VkImage> swapchainImages;
std::vector<VkImageView> swapchainImageViews;

VkDescriptorPool descriptorPool;
VkDescriptorSetLayout graphicsDescriptorSetLayout;
VkDescriptorSet graphicsDescriptorSet;
VkDescriptorSetLayout computeDescriptorSetLayout;
VkDescriptorSet computeDescriptorSet;

VkDescriptorPool imguiDescriptorPool;

VkPipeline graphicsPipeline;
VkPipelineLayout graphicsPipelineLayout;

VkPipeline computePipeline;
VkPipelineLayout computePipelineLayout;

struct FrameData
{
    VkSemaphore imageAcquiredSemaphore, renderFinishedSemaphore;
    VkFence renderFinishedFence;

    VkCommandPool commandPool;
    VkCommandBuffer mainCommandBuffer;

    AllocatedBuffer cameraBuffer;
    VkDescriptorSet globalDescriptor;
    AllocatedBuffer objectBuffer;
    VkDescriptorSet objectDescriptor;
};

FrameData frames[maxFramesInFlight];
Scene scene;

void prepareAssets()
{
    ASSERT(EXISTS(assetsPath) && "Assets must reside in the working dir!");

#ifdef DEBUG
    initShaderCompiler(baseShaderDirPath);
    compileShaderIntoSpv(vertexShaderPath, vertexShaderSpvPath, shaderc_vertex_shader);
    compileShaderIntoSpv(fragmentShaderPath, fragmentShaderSpvPath, shaderc_fragment_shader);
    compileShaderIntoSpv(computeShaderPath, computeShaderSpvPath, shaderc_compute_shader);
    terminateShaderCompiler();

    if (!EXISTS(lanternSceneDataPath))
    {
        writeSceneToFile(loadSceneFromGltf(lanternGlbPath), lanternSceneDataPath);
    }
#endif // DEBUG

    scene = loadSceneFromFile(lanternSceneDataPath);
}

void initVulkan()
{
    vkVerify(volkInitialize());

    vkb::InstanceBuilder builder;
    vkb::Instance vkbInstance = builder
        .set_app_name(appName)
        .require_api_version(1, 2, 0)
#ifdef DEBUG
        .request_validation_layers(true)
        .use_default_debug_messenger()
#endif // DEBUG
        .build()
        .value();
    instance = vkbInstance.instance;
    debugMessenger = vkbInstance.debug_messenger;

    volkLoadInstanceOnly(instance);

    vkVerify(glfwCreateWindowSurface(instance, window, nullptr, &surface));

    VkPhysicalDeviceVulkan11Features features11 {};
    features11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    features11.storageBuffer16BitAccess = true;
    features11.uniformAndStorageBuffer16BitAccess = true;

    VkPhysicalDeviceVulkan12Features features12 {};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.storageBuffer8BitAccess = true;
    features12.uniformAndStorageBuffer8BitAccess = true;

    vkb::PhysicalDeviceSelector selector {vkbInstance, surface};
    vkb::PhysicalDevice vkbPhysicalDevice = selector
        .set_minimum_version(1, 2)
        .add_required_extension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME)
        .add_required_extension(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME)
        .set_required_features_11(features11)
        .set_required_features_12(features12)
        .select()
        .value();

    VkPhysicalDeviceSynchronization2FeaturesKHR synchronization2Features {};
    synchronization2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR;
    synchronization2Features.synchronization2 = true;

    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeatures {};
    dynamicRenderingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
    dynamicRenderingFeatures.dynamicRendering = true;

    vkb::DeviceBuilder deviceBuilder {vkbPhysicalDevice};
    vkb::Device vkbDevice = deviceBuilder
        .add_pNext(&synchronization2Features)
        .add_pNext(&dynamicRenderingFeatures)
        .build()
        .value();

    physicalDevice = vkbPhysicalDevice.physical_device;
    physicalDeviceProperties = vkbPhysicalDevice.properties;
    device = vkbDevice.device;

    volkLoadDevice(device);

    graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    VmaVulkanFunctions vulkanFunctions = initVmaVulkanFunctions();
    VmaAllocatorCreateInfo allocatorCreateInfo {};
    allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_2;
    allocatorCreateInfo.physicalDevice = physicalDevice;
    allocatorCreateInfo.device = device;
    allocatorCreateInfo.instance = instance;
    allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;
    vkVerify(vmaCreateAllocator(&allocatorCreateInfo, &allocator));
}

void terminateVulkan()
{
    vmaDestroyAllocator(allocator);
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkb::destroy_debug_utils_messenger(instance, debugMessenger);
    vkDestroyInstance(instance, nullptr);
    volkFinalize();
}

void createSwapchain(uint32_t width, uint32_t height)
{
    vkb::SwapchainBuilder swapchainBuilder {physicalDevice, device, surface};

    VkSurfaceFormatKHR surfaceFormat;
    surfaceFormat.format = VK_FORMAT_B8G8R8A8_SRGB;
    surfaceFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

    vkb::Swapchain vkbSwapchain = swapchainBuilder
        .set_desired_format(surfaceFormat)
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_extent(width, height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .build()
        .value();

    swapchain = vkbSwapchain.swapchain;
    swapchainImages = vkbSwapchain.get_images().value();
    swapchainImageViews = vkbSwapchain.get_image_views().value();
    swapchainImageFormat = vkbSwapchain.image_format;
    swapchainImageExtent = vkbSwapchain.extent;

    //depthFormat = VK_FORMAT_D32_SFLOAT;
    //VkExtent3D depthImageExtent {windowExtent.width, windowExtent.height, 1};
    //VkImageCreateInfo depthImageInfo = vkinit::image_create_info(depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depthImageExtent);
    //VmaAllocationCreateInfo depthImageAllocInfo {};
    //depthImageAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    //depthImageAllocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    //vkVerify(vmaCreateImage(allocator, &depthImageInfo, &depthImageAllocInfo, &depthImage.image, &depthImage.allocation, nullptr));

    //VkImageViewCreateInfo depthViewInfo = vkinit::imageview_create_info(depthFormat, depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);
    //vkVerify(vkCreateImageView(device, &depthViewInfo, nullptr, &depthImageView));
}

void destroySwapchain()
{
    vkDestroySwapchainKHR(device, swapchain, nullptr);

    for (int i = 0; i < swapchainImageViews.size(); i++)
    {
        vkDestroyImageView(device, swapchainImageViews[i], nullptr);
    }
}

void initRenderTargets()
{
    createSwapchain(windowExtent.width, windowExtent.height);
}

void terminateRenderTargets()
{
    destroySwapchain();
}

void initFramesData()
{
    VkCommandPoolCreateInfo commandPoolCreateInfo = initCommandPoolCreateInfo(graphicsQueueFamily);
    VkFenceCreateInfo fenceCreateInfo = initFenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo semaphoreCreateInfo = initSemaphoreCreateInfo();

    for (FrameData &frame : frames)
    {
        vkVerify(vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &frame.commandPool));
        VkCommandBufferAllocateInfo commandBufferAllocateInfo = initCommandBufferAllocateInfo(frame.commandPool, 1);
        vkVerify(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &frame.mainCommandBuffer));

        vkVerify(vkCreateFence(device, &fenceCreateInfo, nullptr, &frame.renderFinishedFence));
        vkVerify(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &frame.imageAcquiredSemaphore));
        vkVerify(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &frame.renderFinishedSemaphore));
    }
}

void terminateFramesData()
{
    for (FrameData &frame : frames)
    {
        vkDestroyCommandPool(device, frame.commandPool, nullptr);
        vkDestroyFence(device, frame.renderFinishedFence, nullptr);
        vkDestroySemaphore(device, frame.imageAcquiredSemaphore, nullptr);
        vkDestroySemaphore(device, frame.renderFinishedSemaphore, nullptr);
    }
}

void initDescriptors()
{
    VkDescriptorPoolSize poolSizes[]
    {
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 16}
    };
    VkDescriptorPoolCreateInfo poolCreateInfo = initDescriptorPoolCreateInfo(10, poolSizes, COUNTOF(poolSizes));
    vkVerify(vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &descriptorPool));

    // Graphics
    VkDescriptorSetLayoutBinding binding0 {};
    binding0.binding = 0;
    binding0.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binding0.descriptorCount = 1;
    binding0.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    VkDescriptorSetLayoutBinding binding1 = binding0;
    binding1.binding = 1;
    VkDescriptorSetLayoutBinding binding2 = binding0;
    binding2.binding = 2;

    VkDescriptorSetLayoutBinding bindings[] {binding0, binding1, binding2};
    VkDescriptorSetLayoutCreateInfo graphicsSetLayoutCreateInfo = initDescriptorSetLayoutCreateInfo(bindings, COUNTOF(bindings));
    vkVerify(vkCreateDescriptorSetLayout(device, &graphicsSetLayoutCreateInfo, nullptr, &graphicsDescriptorSetLayout));

    VkDescriptorSetAllocateInfo graphicsSetAllocateInfo = initDescriptorSetAllocateInfo(descriptorPool, &graphicsDescriptorSetLayout, 1);
    vkVerify(vkAllocateDescriptorSets(device, &graphicsSetAllocateInfo, &graphicsDescriptorSet));

    // Compute
    VkDescriptorSetLayoutCreateInfo computeSetLayoutCreateInfo = initDescriptorSetLayoutCreateInfo(nullptr, 0);
    vkVerify(vkCreateDescriptorSetLayout(device, &computeSetLayoutCreateInfo, nullptr, &computeDescriptorSetLayout));

    VkDescriptorSetAllocateInfo computeSetAllocateInfo = initDescriptorSetAllocateInfo(descriptorPool, &computeDescriptorSetLayout, 1);
    vkVerify(vkAllocateDescriptorSets(device, &computeSetAllocateInfo, &computeDescriptorSet));
}

void terminateDescriptors()
{
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device, graphicsDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, computeDescriptorSetLayout, nullptr);
}

void initPipelines()
{
    // Graphics
    VkPipelineLayoutCreateInfo graphicsPipelineLayoutCreateInfo = initPipelineLayoutCreateInfo(&graphicsDescriptorSetLayout, 1);
    vkVerify(vkCreatePipelineLayout(device, &graphicsPipelineLayoutCreateInfo, nullptr, &graphicsPipelineLayout));

    VkPipelineColorBlendAttachmentState blendState {};
    blendState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkShaderModule vertexShader = createShaderModuleFromSpv(device, vertexShaderSpvPath);
    VkShaderModule fragmentShader = createShaderModuleFromSpv(device, fragmentShaderSpvPath);
    VkPipelineShaderStageCreateInfo stages[]
    {
        initPipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vertexShader),
        initPipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader)
    };

    VkPipelineVertexInputStateCreateInfo vertexInputState = initPipelineVertexInputStateCreateInfo(0, nullptr, 0, nullptr);
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = initPipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    VkPipelineViewportStateCreateInfo viewportState = initPipelineViewportStateCreateInfo();
    VkPipelineRasterizationStateCreateInfo rasterizationState = initPipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    VkPipelineMultisampleStateCreateInfo multisampleState = initPipelineMultisampleStateCreateInfo();
    VkPipelineDepthStencilStateCreateInfo depthStencilState = initPipelineDepthStencilStateCreateInfo(false, false, false);
    VkPipelineColorBlendStateCreateInfo colorBlendState = initPipelineColorBlendStateCreateInfo(&blendState, 1);
    VkPipelineDynamicStateCreateInfo dynamicState = initPipelineDynamicStateCreateInfo();
    VkPipelineRenderingCreateInfoKHR renderingInfo = initPipelineRenderingCreateInfo(&swapchainImageFormat, 1);
    VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = initGraphicsPipelineCreateInfo(graphicsPipelineLayout,
        COUNTOF(stages),
        stages,
        &vertexInputState,
        &inputAssemblyState,
        &viewportState,
        &rasterizationState,
        &multisampleState,
        &depthStencilState,
        &colorBlendState,
        &dynamicState,
        &renderingInfo);
    vkVerify(vkCreateGraphicsPipelines(device, nullptr, 1, &graphicsPipelineCreateInfo, nullptr, &graphicsPipeline));
    vkDestroyShaderModule(device, vertexShader, nullptr);
    vkDestroyShaderModule(device, fragmentShader, nullptr);

    // Compute
    VkPipelineLayoutCreateInfo computePipelineLayoutCreateInfo = initPipelineLayoutCreateInfo(&computeDescriptorSetLayout, 1);
    vkVerify(vkCreatePipelineLayout(device, &computePipelineLayoutCreateInfo, nullptr, &computePipelineLayout));
    VkShaderModule computeShader = createShaderModuleFromSpv(device, computeShaderSpvPath);
    VkPipelineShaderStageCreateInfo computeShaderStageCreateInfo = initPipelineShaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT, computeShader);
    VkComputePipelineCreateInfo computePipelineCreateInfo = initComputePipelineCreateInfo(computeShaderStageCreateInfo, computePipelineLayout);
    vkVerify(vkCreateComputePipelines(device, nullptr, 1, &computePipelineCreateInfo, nullptr, &computePipeline));
    vkDestroyShaderModule(device, computeShader, nullptr);
}

void terminatePipelines()
{
    vkDestroyPipelineLayout(device, graphicsPipelineLayout, nullptr);
    vkDestroyPipeline(device, graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(device, computePipelineLayout, nullptr);
    vkDestroyPipeline(device, computePipeline, nullptr);
}

struct ImguiVulkanContext
{
    VkInstance instance;
    VkDevice device;
};

static PFN_vkVoidFunction imguiVulkanLoader(const char *funcName, void *userData)
{
    ImguiVulkanContext *context = (ImguiVulkanContext *)userData;
    PFN_vkVoidFunction instanceProcAddr = vkGetInstanceProcAddr(context->instance, funcName);
    PFN_vkVoidFunction deviceProcAddr = vkGetDeviceProcAddr(context->device, funcName);
    return deviceProcAddr ? deviceProcAddr : instanceProcAddr;
}

void initImgui()
{
    VkDescriptorPoolSize poolSizes[]
    {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
    };

    VkDescriptorPoolCreateInfo poolCreateInfo = initDescriptorPoolCreateInfo(1, poolSizes, COUNTOF(poolSizes), VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);
    vkVerify(vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &imguiDescriptorPool));

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForVulkan(window, true);

    static ImguiVulkanContext imguiVulkanContext;
    imguiVulkanContext.instance = instance;
    imguiVulkanContext.device = device;
    ImGui_ImplVulkan_LoadFunctions(imguiVulkanLoader, &imguiVulkanContext);

    ImGui_ImplVulkan_InitInfo imguiInitInfo {};
    imguiInitInfo.Instance = instance;
    imguiInitInfo.PhysicalDevice = physicalDevice;
    imguiInitInfo.Device = device;
    imguiInitInfo.QueueFamily = graphicsQueueFamily;
    imguiInitInfo.Queue = graphicsQueue;
    imguiInitInfo.DescriptorPool = imguiDescriptorPool;
    imguiInitInfo.MinImageCount = maxFramesInFlight;
    imguiInitInfo.ImageCount = maxFramesInFlight;
    imguiInitInfo.UseDynamicRendering = true;
    imguiInitInfo.PipelineRenderingCreateInfo = initPipelineRenderingCreateInfo(&swapchainImageFormat, 1);
    ImGui_ImplVulkan_Init(&imguiInitInfo);

    ImGui::GetIO().IniFilename = nullptr; // disable imgui.ini
}

void terminateImgui()
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    vkDestroyDescriptorPool(device, imguiDescriptorPool, nullptr);
}

void initScene()
{
    scene.buffer = createBuffer(allocator,
        scene.indexesSize + scene.positionsSize + scene.normalUvsSize + scene.matricesSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    void *data;
    vmaMapMemory(allocator, scene.buffer.allocation, &data);
    memcpy((char *)data + scene.indicesOffset, scene.indices.data(), scene.indexesSize);
    memcpy((char *)data + scene.positionsOffset, scene.positions.data(), scene.positionsSize);
    memcpy((char *)data + scene.normalUvsOffset, scene.normalUvs.data(), scene.normalUvsSize);
    memcpy((char *)data + scene.matricesOffset, scene.matrices.data(), scene.matricesSize);
    vmaUnmapMemory(allocator, scene.buffer.allocation);

    VkDescriptorBufferInfo bufferInfo0 {};
    bufferInfo0.buffer = scene.buffer.buffer;
    bufferInfo0.offset = scene.positionsOffset;
    bufferInfo0.range = scene.positionsSize;
    VkDescriptorBufferInfo bufferInfo1 {};
    bufferInfo1.buffer = scene.buffer.buffer;
    bufferInfo1.offset = scene.normalUvsOffset;
    bufferInfo1.range = scene.normalUvsSize;
    VkDescriptorBufferInfo bufferInfo2 {};
    bufferInfo2.buffer = scene.buffer.buffer;
    bufferInfo2.offset = scene.matricesOffset;
    bufferInfo2.range = scene.matricesSize;
    VkWriteDescriptorSet writes[]
    {
        initWriteDescriptorSetBuffer(graphicsDescriptorSet, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferInfo0),
        initWriteDescriptorSetBuffer(graphicsDescriptorSet, 1, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferInfo1),
        initWriteDescriptorSetBuffer(graphicsDescriptorSet, 2, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferInfo2)
    };
    vkUpdateDescriptorSets(device, COUNTOF(writes), writes, 0, nullptr);
}

void terminateScene()
{
    vmaDestroyBuffer(allocator, scene.buffer.buffer, scene.buffer.allocation);
}

void init()
{
    prepareAssets();

    VERIFY(glfwInit());
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window = glfwCreateWindow(windowExtent.width, windowExtent.height, appName, nullptr, nullptr);
    ASSERT(window);

    initVulkan();
    initRenderTargets();
    initFramesData();
    initDescriptors();
    initPipelines();
    initImgui();

    initScene();
}

void exit()
{
    vkDeviceWaitIdle(device);

    terminateScene();

    terminateImgui();
    terminatePipelines();
    terminateDescriptors();
    terminateFramesData();
    terminateRenderTargets();
    terminateVulkan();

    glfwDestroyWindow(window);
    glfwTerminate();
}

void update()
{
}

void clearRenderTargets(VkCommandBuffer cmd)
{
    VkClearAttachment clearAttachments[]
    {
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0},
        {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0}
    };
    VkClearRect clearRects[]
    {
        {{{0,0}, swapchainImageExtent}, 0, 1},
        {{{0,0}, swapchainImageExtent}, 0, 1}
    };
    vkCmdClearAttachments(cmd, COUNTOF(clearAttachments), clearAttachments, COUNTOF(clearRects), clearRects);
}

void drawGeometry(VkCommandBuffer cmd)
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

    VkViewport viewport {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = (float)swapchainImageExtent.width;
    viewport.height = (float)swapchainImageExtent.height;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = swapchainImageExtent.width;
    scissor.extent.height = swapchainImageExtent.height;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindIndexBuffer(cmd, scene.buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineLayout, 0, 1, &graphicsDescriptorSet, 0, nullptr);
    vkCmdDrawIndexed(cmd, (uint32_t)scene.indices.size(), 1, 0, 0, 0);
}

void drawUI(VkCommandBuffer cmd)
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::ShowDemoWindow();

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

void draw()
{
    FrameData &frame = frames[frameIndex];
    VkResult res = vkWaitForFences(device, 1, &frame.renderFinishedFence, true, UINT64_MAX);
    vkVerify(res);
    vkVerify(vkResetFences(device, 1, &frame.renderFinishedFence));

    uint32_t swapchainImageIndex;
    vkVerify(vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, frame.imageAcquiredSemaphore, nullptr, &swapchainImageIndex));

    vkVerify(vkResetCommandPool(device, frame.commandPool, 0));
    VkCommandBuffer cmd = frame.mainCommandBuffer;
    VkCommandBufferBeginInfo commandBufferBeginInfo = initCommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    vkVerify(vkBeginCommandBuffer(cmd, &commandBufferBeginInfo));

    VkRenderingAttachmentInfoKHR colorAttachmentInfo = initRenderingAttachmentInfo(swapchainImageViews[swapchainImageIndex]);
    VkRenderingInfoKHR renderingInfo = initRenderingInfo(swapchainImageExtent, &colorAttachmentInfo, 1);

    transitionImage(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    vkCmdBeginRenderingKHR(cmd, &renderingInfo);
    clearRenderTargets(cmd);
    drawGeometry(cmd);
    drawUI(cmd);
    vkCmdEndRenderingKHR(cmd);
    transitionImage(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    vkVerify(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfoKHR cmdSubmitInfo = initCommandBufferSubmitInfo(cmd);
    VkSemaphoreSubmitInfoKHR waitSemaphoreSubmitInfo = initSemaphoreSubmitInfo(frame.imageAcquiredSemaphore, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR);
    VkSemaphoreSubmitInfoKHR signalSemaphoreSubmitInfo = initSemaphoreSubmitInfo(frame.renderFinishedSemaphore, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT);
    VkSubmitInfo2 submitInfo = initSubmitInfo(&cmdSubmitInfo, &waitSemaphoreSubmitInfo, &signalSemaphoreSubmitInfo);
    vkVerify(vkQueueSubmit2KHR(graphicsQueue, 1, &submitInfo, frame.renderFinishedFence));

    VkPresentInfoKHR presentInfo = initPresentInfo(&swapchain, &frame.renderFinishedSemaphore, &swapchainImageIndex);
    vkVerify(vkQueuePresentKHR(graphicsQueue, &presentInfo));

    frameIndex = (frameIndex + 1) % maxFramesInFlight;
}