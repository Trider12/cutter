#include <cwalk.h>
#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <meshoptimizer.h>
#include <VkBootstrap.h>
#define TRACY_VK_USE_SYMBOL_TABLE
#include <tracy/Tracy.hpp>
#include <tracy/TracyVulkan.hpp>

#include "Camera.hpp"
#include "DebugUtils.hpp"
#include "Graphics.hpp"
#include "ImageUtils.hpp"
#include "JobSystem.hpp"
#include "Scene.hpp"
#include "ShaderUtils.hpp"
#include "VkUtils.hpp"

// skip importing if already exist
#define SKIP_SCENE_REIMPORT 1
#define SKIP_MATERIAL_REIMPORT 1
#define SKIP_ENV_MAP_REIMPORT 1

GLFWwindow *window;

void init();
void update(float delta);
void draw();
void exit();

int main()
{
    const float defaultDelta = 1.f / 60.f;
    float delta = defaultDelta;
    uint64_t prevTime = glfwGetTimerValue();

    init();

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        update(delta);
        draw();

        uint64_t currTime = glfwGetTimerValue();
        delta = (float)(currTime - prevTime) / glfwGetTimerFrequency();
        if (delta > 1.f)
            delta = defaultDelta;
        prevTime = currTime;
        FrameMark;
    }

    exit();

    return 0;
}

const char *const appName = "Cutter";

#define shadersPath  "src/shaders/"

#define assetsPath   "assets/"
#define spvsPath     assetsPath "spv/"
#define scenesPath   assetsPath "scenes/"
#define fontsPath    assetsPath "fonts/"
#define envmapsPath  assetsPath "envmaps/"
#define texturesPath assetsPath "textures/"

#define contentPath    "content/"
#define glbsPath       contentPath "glb/"
#define imagesPath     contentPath "images/"
#define hdriImagesPath contentPath "hdri/"

const struct ShaderCompileInfo
{
    const char *shaderSourcePath;
    const char *shaderSpvPath;
    ShaderType shaderType;
} shaderInfos[]
{
    {shadersPath"basic.vert",                 spvsPath"basic.vert.spv",                 ShaderType::Vertex},
    {shadersPath"basic.frag",                 spvsPath"basic.frag.spv",                 ShaderType::Fragment},
    {shadersPath"drawSkybox.vert",            spvsPath"drawSkybox.vert.spv",            ShaderType::Vertex},
    {shadersPath"drawSkybox.frag",            spvsPath"drawSkybox.frag.spv",            ShaderType::Fragment},
    {shadersPath"computeSkybox.comp",         spvsPath"computeSkybox.comp.spv",         ShaderType::Compute},
    {shadersPath"computeBrdfLut.comp",        spvsPath"computeBrdfLut.comp.spv",        ShaderType::Compute},
    {shadersPath"computeIrradianceMap.comp",  spvsPath"computeIrradianceMap.comp.spv",  ShaderType::Compute},
    {shadersPath"computePrefilteredMap.comp", spvsPath"computePrefilteredMap.comp.spv", ShaderType::Compute},
    {shadersPath"compute.comp",               spvsPath"compute.comp.spv",               ShaderType::Compute}
};

const struct SceneImportInfo
{
    const char *glbAssetPath;
    const char *sceneDirPath;
    float scalingFactor;
} sceneInfos[]
{
    {glbsPath"Cube.glb",              scenesPath"Cube",              1.f},
    {glbsPath"ShaderBall.glb",        scenesPath"ShaderBall",        1.f},
    {glbsPath"Lantern.glb",           scenesPath"Lantern",           0.2f},
    {glbsPath"WaterBottle.glb",       scenesPath"WaterBottle",       8.f},
    {glbsPath"SciFiHelmet.glb",       scenesPath"SciFiHelmet",       1.f},
    {glbsPath"NormalTangentTest.glb", scenesPath"NormalTangentTest", 1.f}
};
uint8_t selectedScene = 1;
bool sceneChangeRequired = false;

const struct MaterialImportInfo
{
    MaterialTextureSet srcSet;
    MaterialTextureSet dstSet;
} materialInfos[]
{
    {
        imagesPath"used-stainless-steel2-bl/used-stainless-steel2_albedo.png",
        imagesPath"used-stainless-steel2-bl/used-stainless-steel2_normal-ogl.png",
        imagesPath"used-stainless-steel2-bl/used-stainless-steel2_ao-roughness-metallic.png",
        texturesPath"used-stainless-steel/used-stainless-steel_basecolor.ktx2",
        texturesPath"used-stainless-steel/used-stainless-steel_normal.ktx2",
        texturesPath"used-stainless-steel/used-stainless-steel_ao-roughness-metallic.ktx2",
    },
    {
        imagesPath"white-marble-bl/white-marble_albedo.png",
        imagesPath"white-marble-bl/white-marble_normal-ogl.png",
        imagesPath"white-marble-bl/white-marble_ao-roughness-metallic.png",
        texturesPath"white-marble/white-marble_basecolor.ktx2",
        texturesPath"white-marble/white-marble_normal.ktx2",
        texturesPath"white-marble/white-marble_ao-roughness-metallic.ktx2",
    },
    {
        imagesPath"brick-wall-bl/brick-wall_albedo.png",
        imagesPath"brick-wall-bl/brick-wall_normal-ogl.png",
        imagesPath"brick-wall-bl/brick-wall_ao-roughness-metallic.png",
        texturesPath"brick-wall/brick-wall_basecolor.ktx2",
        texturesPath"brick-wall/brick-wall_normal.ktx2",
        texturesPath"brick-wall/brick-wall_ao-roughness-metallic.ktx2",
    },
    {
        imagesPath"rustediron1-alt2-bl/rustediron2_basecolor.png",
        imagesPath"rustediron1-alt2-bl/rustediron2_normal.png",
        imagesPath"rustediron1-alt2-bl/rustediron2_ao-roughness-metallic.png",
        texturesPath"rustediron/rustediron_basecolor.ktx2",
        texturesPath"rustediron/rustediron_normal.ktx2",
        texturesPath"rustediron/rustediron_ao-roughness-metallic.ktx2",
    },
    {
        imagesPath"copper-rock1-bl/copper-rock1-alb.png",
        imagesPath"copper-rock1-bl/copper-rock1-normal.png",
        imagesPath"copper-rock1-bl/copper-rock1-ao-roughness-metallic.png",
        texturesPath"copper-rock/copper-rock_basecolor.ktx2",
        texturesPath"copper-rock/copper-rock_normal.ktx2",
        texturesPath"copper-rock/copper-rock_ao-roughness-metallic.ktx2",
    }
};
const char *const materialNames[]
{
    "Scene Default",
    "Stainless Steel",
    "White Marble",
    "Brick Wall",
    "Rusted Iron",
    "Copper Rock"
};
static_assert(COUNTOF(materialInfos) + 1 == COUNTOF(materialNames), "");
uint32_t selectedMaterial = 0;

const char *const hdriImagePaths[]
{
    hdriImagesPath"fish_hoek_beach_4k.hdr",
    hdriImagesPath"lilienstein_4k.hdr",
    hdriImagesPath"HDR_040_Field_Ref.hdr"
};

const char *const brdfLutImagePath = envmapsPath"brdfLut.ktx2";

const char *const defaultFontPath = fontsPath"Roboto-Medium.ttf";
const uint8_t defaultFontSize = 16;

VkExtent2D windowExtent;
float uiScale;

VkInstance instance;
VkDebugUtilsMessengerEXT debugMessenger;
VkPhysicalDevice physicalDevice;
VkPhysicalDeviceProperties physicalDeviceProperties;
VkDevice device;
VkSurfaceKHR surface;
TracyVkCtx tracyContext;

VkQueue graphicsQueue;
uint32_t graphicsQueueFamilyIndex;
VkQueue computeQueue;
uint32_t computeQueueFamilyIndex;
VkQueue transferQueue;
uint32_t transferQueueFamilyIndex;

VkSwapchainKHR swapchain;
VkFormat swapchainImageFormat;
VkExtent2D swapchainImageExtent;
std::vector<VkImage> swapchainImages;
std::vector<VkImageView> swapchainImageViews;
GpuImage depthImage;
bool resizeNeeded;

VkDescriptorPool descriptorPool;
VkDescriptorSetLayout globalDescriptorSetLayout;
VkDescriptorSet globalDescriptorSet;
VkDescriptorSetLayout texturesDescriptorSetLayout;
VkDescriptorSet texturesDescriptorSet;
VkDescriptorSetLayout skyboxDescriptorSetLayout;
VkDescriptorSet skyboxDescriptorSet;
VkDescriptorSetLayout computeDescriptorSetLayout;
VkDescriptorSet computeDescriptorSet;

VkPipeline modelPipeline;
VkPipelineLayout modelPipelineLayout;
VkPipeline skyboxPipeline;
VkPipelineLayout skyboxPipelineLayout;
VkPipeline computePipeline;
VkPipelineLayout computePipelineLayout;

GpuBuffer modelBuffer;
GpuBuffer skyboxBuffer;
GpuBuffer globalUniformBuffer;

GpuImage modelTextures[MAX_MODEL_TEXTURES];
uint8_t modelTextureCount;
GpuImage skyboxImages[COUNTOF(hdriImagePaths)];
GpuImage irradianceMaps[COUNTOF(hdriImagePaths)];
GpuImage prefilteredMaps[COUNTOF(hdriImagePaths)];
GpuImage brdfLut;
uint32_t selectedSkybox = 0;

VkSampler linearRepeatSampler;

struct FrameData
{
    VkSemaphore imageAcquiredSemaphore, renderFinishedSemaphore;
    VkFence renderFinishedFence;

    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;

    CameraData cameraData;
};

const uint8_t maxFramesInFlight = 2;
uint8_t frameIndex = 0;
FrameData frames[maxFramesInFlight];

FlyCamera camera;
Scene model;
Scene skybox;
LightingData lightData;

bool rotateScene = false;
float sceneRotationTime = 0.f;
uint32_t sceneConfig = SCENE_USE_LIGHTS | SCENE_USE_IBL;
glm::vec4 lightDirsAndIntensitiesUI[MAX_LIGHTS];

glm::vec2 prevCursorPos;
bool firstCursorUpdate = true;
bool cursorCaptured = false;

uint32_t debugFlags;

void initVulkan()
{
    ZoneScoped;
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

    VkPhysicalDeviceFeatures features {};
    features.shaderSampledImageArrayDynamicIndexing = true;
    features.textureCompressionBC = true;
    VkPhysicalDeviceVulkan11Features features11 {};
    features11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    features11.storageBuffer16BitAccess = true;
    features11.uniformAndStorageBuffer16BitAccess = true;
    VkPhysicalDeviceVulkan12Features features12 {};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.storageBuffer8BitAccess = true;
    features12.uniformAndStorageBuffer8BitAccess = true;
    features12.uniformBufferStandardLayout = true;
    features12.hostQueryReset = true;
    features12.runtimeDescriptorArray = true;
    features12.descriptorBindingPartiallyBound = true;

    vkb::PhysicalDeviceSelector selector { vkbInstance, surface };
    vkb::PhysicalDevice vkbPhysicalDevice = selector
        .set_minimum_version(1, 2)
        .add_required_extension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME)
        .add_required_extension(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME)
        .add_required_extension(VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME)
        .set_required_features(features)
        .set_required_features_11(features11)
        .set_required_features_12(features12)
        .select(vkb::DeviceSelectionMode::only_fully_suitable)
        .value();

    VkPhysicalDeviceSynchronization2FeaturesKHR synchronization2Features {};
    synchronization2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR;
    synchronization2Features.synchronization2 = true;
    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeatures {};
    dynamicRenderingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
    dynamicRenderingFeatures.dynamicRendering = true;

    vkb::DeviceBuilder deviceBuilder { vkbPhysicalDevice };
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
    graphicsQueueFamilyIndex = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
    computeQueue = vkbDevice.get_queue(vkb::QueueType::compute).value();
    computeQueueFamilyIndex = vkbDevice.get_queue_index(vkb::QueueType::compute).value();
    transferQueue = vkbDevice.get_queue(vkb::QueueType::transfer).value();
    transferQueueFamilyIndex = vkbDevice.get_queue_index(vkb::QueueType::transfer).value();

    tracyContext = TracyVkContextHostCalibrated(instance, physicalDevice, device, vkGetInstanceProcAddr, vkGetDeviceProcAddr);
}

void terminateVulkan()
{
    TracyVkDestroy(tracyContext);

    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
#ifdef DEBUG
    vkb::destroy_debug_utils_messenger(instance, debugMessenger);
#endif // DEBUG
    vkDestroyInstance(instance, nullptr);
    volkFinalize();
}

void createSwapchain(uint32_t width, uint32_t height)
{
    ZoneScoped;
    vkb::SwapchainBuilder swapchainBuilder { physicalDevice, device, surface };

    VkSurfaceFormatKHR surfaceFormat;
    surfaceFormat.format = VK_FORMAT_B8G8R8A8_SRGB;
    surfaceFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

    vkb::Swapchain vkbSwapchain = swapchainBuilder
        .set_desired_format(surfaceFormat)
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_extent(width, height)
        .use_default_image_usage_flags()
        .build()
        .value();

    swapchain = vkbSwapchain.swapchain;
    swapchainImages = vkbSwapchain.get_images().value();
    swapchainImageViews = vkbSwapchain.get_image_views().value();
    swapchainImageFormat = vkbSwapchain.image_format;
    swapchainImageExtent = vkbSwapchain.extent;
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
    ZoneScoped;
    createSwapchain(windowExtent.width, windowExtent.height);
    depthImage = createGpuImage2D(VK_FORMAT_D32_SFLOAT,
        windowExtent, 1,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT);
}

void terminateRenderTargets()
{
    destroySwapchain();
    destroyGpuImage(depthImage);
}

void recreateRenderTargets()
{
    ZoneScoped;
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);

    while (width == 0 || height == 0)
    {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

    windowExtent = { (uint32_t)width, (uint32_t)height };
    vkDeviceWaitIdle(device);
    terminateRenderTargets();
    initRenderTargets();
}

void initFrameData()
{
    ZoneScoped;
    VkCommandPoolCreateInfo commandPoolCreateInfo = initCommandPoolCreateInfo(graphicsQueueFamilyIndex, false);
    VkCommandBufferAllocateInfo commandBufferAllocateInfo;
    VkFenceCreateInfo fenceCreateInfo = initFenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo semaphoreCreateInfo = initSemaphoreCreateInfo();

    for (FrameData &frame : frames)
    {
        vkVerify(vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &frame.commandPool));
        commandBufferAllocateInfo = initCommandBufferAllocateInfo(frame.commandPool, 1);
        vkVerify(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &frame.commandBuffer));

        vkVerify(vkCreateFence(device, &fenceCreateInfo, nullptr, &frame.renderFinishedFence));
        vkVerify(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &frame.imageAcquiredSemaphore));
        vkVerify(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &frame.renderFinishedSemaphore));
    }
}

void terminateFrameData()
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
    ZoneScoped;
    VkSamplerCreateInfo samplerCreateInfo = initSamplerCreateInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);
    vkVerify(vkCreateSampler(device, &samplerCreateInfo, nullptr, &linearRepeatSampler));

    VkDescriptorPoolSize poolSizes[]
    {
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 16},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 16},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 16},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 16 + MAX_MODEL_TEXTURES},
        {VK_DESCRIPTOR_TYPE_SAMPLER, 16},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1} // for imgui
    };
    VkDescriptorPoolCreateInfo poolCreateInfo = initDescriptorPoolCreateInfo(10, poolSizes, COUNTOF(poolSizes), VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT /* for imgui */);
    vkVerify(vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &descriptorPool));

    VkDescriptorSetLayoutBinding globalSetBindings[]
    {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
        {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
        {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {5, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {6, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
        {7, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {8, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, COUNTOF(irradianceMaps), VK_SHADER_STAGE_FRAGMENT_BIT},
        {9, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, COUNTOF(prefilteredMaps), VK_SHADER_STAGE_FRAGMENT_BIT},
        {10, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, &linearRepeatSampler}
    };

    VkDescriptorSetLayoutBinding texturesSetBinding { 0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MAX_MODEL_TEXTURES, VK_SHADER_STAGE_FRAGMENT_BIT };
    VkDescriptorSetLayoutBinding skyboxSetBindings[]
    {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
        {1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, COUNTOF(skyboxImages), VK_SHADER_STAGE_FRAGMENT_BIT}
    };

    VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo = initDescriptorSetLayoutCreateInfo(globalSetBindings, COUNTOF(globalSetBindings));
    vkVerify(vkCreateDescriptorSetLayout(device, &setLayoutCreateInfo, nullptr, &globalDescriptorSetLayout));
    setLayoutCreateInfo = initDescriptorSetLayoutCreateInfo(&texturesSetBinding, 1);
    vkVerify(vkCreateDescriptorSetLayout(device, &setLayoutCreateInfo, nullptr, &texturesDescriptorSetLayout));
    setLayoutCreateInfo = initDescriptorSetLayoutCreateInfo(skyboxSetBindings, COUNTOF(skyboxSetBindings));
    vkVerify(vkCreateDescriptorSetLayout(device, &setLayoutCreateInfo, nullptr, &skyboxDescriptorSetLayout));
    setLayoutCreateInfo = initDescriptorSetLayoutCreateInfo(nullptr, 0);
    vkVerify(vkCreateDescriptorSetLayout(device, &setLayoutCreateInfo, nullptr, &computeDescriptorSetLayout));

    VkDescriptorSetLayout layouts[] { globalDescriptorSetLayout, texturesDescriptorSetLayout, skyboxDescriptorSetLayout, computeDescriptorSetLayout };
    VkDescriptorSet sets[COUNTOF(layouts)];
    VkDescriptorSetAllocateInfo setAllocateInfo = initDescriptorSetAllocateInfo(descriptorPool, layouts, COUNTOF(layouts));
    vkVerify(vkAllocateDescriptorSets(device, &setAllocateInfo, sets));
    globalDescriptorSet = sets[0];
    texturesDescriptorSet = sets[1];
    skyboxDescriptorSet = sets[2];
    computeDescriptorSet = sets[3];
}

void terminateDescriptors()
{
    vkDestroySampler(device, linearRepeatSampler, nullptr);
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device, globalDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, texturesDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, skyboxDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, computeDescriptorSetLayout, nullptr);
}

void initPipelines()
{
    ZoneScoped;
    // Model
    VkPushConstantRange range {};
    range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    range.size = 3 * sizeof(uint32_t);
    VkDescriptorSetLayout layouts[] { globalDescriptorSetLayout, texturesDescriptorSetLayout };
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = initPipelineLayoutCreateInfo(layouts, COUNTOF(layouts), &range, 1);
    vkVerify(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &modelPipelineLayout));

    VkPipelineColorBlendAttachmentState blendState {};
    blendState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkShaderModule vertexShader = createShaderModuleFromSpv(device, shaderInfos[0].shaderSpvPath);
    VkShaderModule fragmentShader = createShaderModuleFromSpv(device, shaderInfos[1].shaderSpvPath);
    VkPipelineShaderStageCreateInfo stages[2];
    stages[0] = initPipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vertexShader);
    stages[1] = initPipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader);

    VkPipelineVertexInputStateCreateInfo vertexInputState = initPipelineVertexInputStateCreateInfo(nullptr, 0, nullptr, 0);
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = initPipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    VkPipelineViewportStateCreateInfo viewportState = initPipelineViewportStateCreateInfo();
    VkPipelineRasterizationStateCreateInfo rasterizationState = initPipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    VkPipelineMultisampleStateCreateInfo multisampleState = initPipelineMultisampleStateCreateInfo();
    VkPipelineDepthStencilStateCreateInfo depthStencilState = initPipelineDepthStencilStateCreateInfo(true, true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    VkPipelineColorBlendStateCreateInfo colorBlendState = initPipelineColorBlendStateCreateInfo(&blendState, 1);
    VkPipelineDynamicStateCreateInfo dynamicState = initPipelineDynamicStateCreateInfo();
    VkPipelineRenderingCreateInfoKHR renderingInfo = initPipelineRenderingCreateInfo(&swapchainImageFormat, 1, depthImage.imageFormat);
    VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = initGraphicsPipelineCreateInfo(modelPipelineLayout,
        stages,
        COUNTOF(stages),
        &vertexInputState,
        &inputAssemblyState,
        &viewportState,
        &rasterizationState,
        &multisampleState,
        &depthStencilState,
        &colorBlendState,
        &dynamicState,
        &renderingInfo);
    vkVerify(vkCreateGraphicsPipelines(device, nullptr, 1, &graphicsPipelineCreateInfo, nullptr, &modelPipeline));
    vkDestroyShaderModule(device, vertexShader, nullptr);
    vkDestroyShaderModule(device, fragmentShader, nullptr);

    // Skybox
    layouts[1] = skyboxDescriptorSetLayout;
    vkVerify(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &skyboxPipelineLayout));
    vertexShader = createShaderModuleFromSpv(device, shaderInfos[2].shaderSpvPath);
    fragmentShader = createShaderModuleFromSpv(device, shaderInfos[3].shaderSpvPath);
    stages[0] = initPipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vertexShader);
    stages[1] = initPipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader);
    rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;
    depthStencilState = initPipelineDepthStencilStateCreateInfo(true, false, VK_COMPARE_OP_GREATER_OR_EQUAL);
    graphicsPipelineCreateInfo.layout = skyboxPipelineLayout;
    vkVerify(vkCreateGraphicsPipelines(device, nullptr, 1, &graphicsPipelineCreateInfo, nullptr, &skyboxPipeline));
    vkDestroyShaderModule(device, vertexShader, nullptr);
    vkDestroyShaderModule(device, fragmentShader, nullptr);

    // Compute
    layouts[1] = computeDescriptorSetLayout;
    vkVerify(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &computePipelineLayout));
    VkShaderModule computeShader = createShaderModuleFromSpv(device, shaderInfos[8].shaderSpvPath);
    VkPipelineShaderStageCreateInfo computeShaderStageCreateInfo = initPipelineShaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT, computeShader);
    VkComputePipelineCreateInfo computePipelineCreateInfo = initComputePipelineCreateInfo(computeShaderStageCreateInfo, computePipelineLayout);
    vkVerify(vkCreateComputePipelines(device, nullptr, 1, &computePipelineCreateInfo, nullptr, &computePipeline));
    vkDestroyShaderModule(device, computeShader, nullptr);
}

void terminatePipelines()
{
    vkDestroyPipelineLayout(device, modelPipelineLayout, nullptr);
    vkDestroyPipeline(device, modelPipeline, nullptr);
    vkDestroyPipelineLayout(device, skyboxPipelineLayout, nullptr);
    vkDestroyPipeline(device, skyboxPipeline, nullptr);
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
    ZoneScoped;
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
    imguiInitInfo.QueueFamily = graphicsQueueFamilyIndex;
    imguiInitInfo.Queue = graphicsQueue;
    imguiInitInfo.DescriptorPool = descriptorPool;
    imguiInitInfo.MinImageCount = maxFramesInFlight;
    imguiInitInfo.ImageCount = maxFramesInFlight;
    imguiInitInfo.UseDynamicRendering = true;
    imguiInitInfo.PipelineRenderingCreateInfo = initPipelineRenderingCreateInfo(&swapchainImageFormat, 1, depthImage.imageFormat);
    ImGui_ImplVulkan_Init(&imguiInitInfo);

    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr; // disable imgui.ini
    io.Fonts->AddFontFromFileTTF(defaultFontPath, defaultFontSize * uiScale);
    ImGui::GetStyle().ScaleAllSizes(uiScale);
}

void terminateImgui()
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void prepareAssets()
{
    ZoneScoped;
    ASSERT(pathExists(assetsPath) && "Assets must reside in the working dir!");

    for (uint8_t i = 0; i < COUNTOF(sceneInfos); i++)
    {
#if SKIP_SCENE_REIMPORT
        if (!pathExists(sceneInfos[i].sceneDirPath))
#endif
            importSceneFromGlb(sceneInfos[i].glbAssetPath, sceneInfos[i].sceneDirPath, sceneInfos[i].scalingFactor);
    }

    for (uint8_t i = 0; i < COUNTOF(materialInfos); i++)
    {
#if SKIP_MATERIAL_REIMPORT
        if (!pathExists(materialInfos[i].dstSet.baseColorTexPath))
#endif
            importMaterial(materialInfos[i].srcSet, materialInfos[i].dstSet);
    }
}

void loadModel(const char *sceneDirPath)
{
    ZoneScoped;
    static const uint32_t maxIndexCount = UINT16_MAX * 32;
    static const uint32_t maxVertexCount = UINT16_MAX * 4;
    static const uint32_t maxTransformCount = UINT8_MAX;
    static const uint32_t maxMaterialCount = UINT8_MAX;
    static const uint32_t maxIndicesSize = maxIndexCount * sizeof(uint32_t);
    static const uint32_t maxPositionsSize = maxVertexCount * sizeof(Position);
    static const uint32_t maxNormalUvsSize = maxVertexCount * sizeof(NormalUv);
    static const uint32_t maxTransformsSize = maxTransformCount * sizeof(TransformData);
    static const uint32_t maxMaterialsSize = maxMaterialCount * sizeof(MaterialData);
    static const uint32_t maxBufferSize = maxIndicesSize + maxPositionsSize + maxNormalUvsSize + maxTransformsSize + maxMaterialsSize;
    static const uint32_t maxIndicesOffset = 0;
    static const uint32_t maxPositionsOffset = maxIndicesOffset + maxIndicesSize;
    static const uint32_t maxNormalUvsOffset = maxPositionsOffset + maxPositionsSize;
    static const uint32_t maxTransformsOffset = maxNormalUvsOffset + maxNormalUvsSize;
    static const uint32_t maxMaterialsOffset = maxTransformsOffset + maxTransformsSize;

    model = loadSceneFromFile(sceneDirPath);

    for (uint8_t j = 0; j < COUNTOF(materialInfos); j++)
    {
        addMaterialToScene(model, materialInfos[j].dstSet);
    }

    uint32_t indicesSize = (uint32_t)model.indices.size() * sizeof(decltype(model.indices)::value_type);
    uint32_t positionsSize = (uint32_t)model.positions.size() * sizeof(decltype(model.positions)::value_type);
    uint32_t normalUvsSize = (uint32_t)model.normalUvs.size() * sizeof(decltype(model.normalUvs)::value_type);
    uint32_t transformsSize = (uint32_t)model.transforms.size() * sizeof(decltype(model.transforms)::value_type);
    uint32_t materialsSize = (uint32_t)model.materials.size() * sizeof(decltype(model.materials)::value_type);
    uint32_t indicesOffset = 0;
    uint32_t positionsOffset = indicesOffset + indicesSize;
    uint32_t normalUvsOffset = positionsOffset + positionsSize;
    uint32_t transformsOffset = normalUvsOffset + normalUvsSize;
    uint32_t materialsOffset = transformsOffset + transformsSize;

    ASSERT(indicesSize <= maxIndicesSize);
    ASSERT(positionsSize <= maxPositionsSize);
    ASSERT(normalUvsSize <= maxNormalUvsSize);
    ASSERT(transformsSize <= maxTransformsSize);
    ASSERT(materialsSize <= maxMaterialsSize);

    if (!modelBuffer.buffer)
    {
        modelBuffer = createGpuBuffer(maxBufferSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            0);
    }

    memcpy((char *)stagingBuffer.mappedData + indicesOffset, model.indices.data(), indicesSize);
    memcpy((char *)stagingBuffer.mappedData + positionsOffset, model.positions.data(), positionsSize);
    memcpy((char *)stagingBuffer.mappedData + normalUvsOffset, model.normalUvs.data(), normalUvsSize);
    memcpy((char *)stagingBuffer.mappedData + transformsOffset, model.transforms.data(), transformsSize);
    memcpy((char *)stagingBuffer.mappedData + materialsOffset, model.materials.data(), materialsSize);

    VkBufferCopy copies[]
    {
        {indicesOffset, maxIndicesOffset, indicesSize},
        {positionsOffset, maxPositionsOffset, positionsSize},
        {normalUvsOffset, maxNormalUvsOffset, normalUvsSize},
        {transformsOffset, maxTransformsOffset, transformsSize},
        {materialsOffset, maxMaterialsOffset, materialsSize}
    };
    copyStagingBufferToBuffer(modelBuffer.buffer, copies, COUNTOF(copies), QueueFamily::Graphics);

    VkDescriptorBufferInfo bufferInfos[]
    {
        {modelBuffer.buffer, maxIndicesOffset, indicesSize},
        {modelBuffer.buffer, maxPositionsOffset, positionsSize},
        {modelBuffer.buffer, maxNormalUvsOffset, normalUvsSize},
        {modelBuffer.buffer, maxTransformsOffset, transformsSize},
        {modelBuffer.buffer, maxMaterialsOffset, materialsSize}
    };

    for (uint8_t i = 0; i < modelTextureCount; i++)
    {
        destroyGpuImage(modelTextures[i]);
    }

    ASSERT(model.imagePaths.size() < UINT8_MAX);
    modelTextureCount = (uint8_t)model.imagePaths.size();
    VkDescriptorImageInfo imageInfos[MAX_MODEL_TEXTURES] {};

    for (uint8_t i = 0; i < modelTextureCount; i++)
    {
        ZoneScopedN("Load Texture");
        ZoneText(model.imagePaths[i].data(), model.imagePaths[i].length());
        Image image = loadImage(model.imagePaths[i].data());
        modelTextures[i] = createAndUploadGpuImage2D(image, QueueFamily::Graphics, VK_IMAGE_USAGE_SAMPLED_BIT);
        freeImage(image);
        imageInfos[i].imageView = modelTextures[i].imageView;
        imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    VkWriteDescriptorSet writes[]
    {
        initWriteDescriptorSetBuffer(globalDescriptorSet, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bufferInfos + 0),
        initWriteDescriptorSetBuffer(globalDescriptorSet, 1, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bufferInfos + 1),
        initWriteDescriptorSetBuffer(globalDescriptorSet, 2, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bufferInfos + 2),
        initWriteDescriptorSetBuffer(globalDescriptorSet, 3, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bufferInfos + 3),
        initWriteDescriptorSetBuffer(globalDescriptorSet, 4, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bufferInfos + 4),
        initWriteDescriptorSetImage(texturesDescriptorSet, 0, modelTextureCount, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, imageInfos)
    };

    vkUpdateDescriptorSets(device, COUNTOF(writes), writes, 0, nullptr);
}

void loadEnvMaps(const char *scenePath)
{
    ZoneScoped;
    skybox = loadSceneFromFile(scenePath);

    uint32_t indexCount = (uint32_t)skybox.indices.size();
    uint32_t indicesSize = indexCount * sizeof(decltype(skybox.indices)::value_type);
    uint32_t positionsSize = (uint32_t)skybox.positions.size() * sizeof(decltype(skybox.positions)::value_type);
    uint32_t bufferSize = indicesSize + positionsSize;
    uint32_t indicesOffset = 0;
    uint32_t positionsOffset = indicesOffset + indicesSize;

    if (!skyboxBuffer.buffer)
    {
        skyboxBuffer = createGpuBuffer(bufferSize,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            0);
    }

    memcpy((char *)stagingBuffer.mappedData + indicesOffset, skybox.indices.data(), indicesSize);
    memcpy((char *)stagingBuffer.mappedData + positionsOffset, skybox.positions.data(), positionsSize);

    VkBufferCopy copies[]
    {
        {indicesOffset, indicesOffset, indicesSize},
        {positionsOffset, positionsOffset, positionsSize}
    };
    copyStagingBufferToBuffer(skyboxBuffer.buffer, copies, COUNTOF(copies), QueueFamily::Graphics);

    VkDescriptorBufferInfo skyboxBufferInfos[]
    {
        {skyboxBuffer.buffer, positionsOffset, positionsSize},
    };

#if SKIP_ENV_MAP_REIMPORT
    if (!pathExists(brdfLutImagePath))
#endif
        computeBrdfLut(brdfLutImagePath);

    Image image = loadImage(brdfLutImagePath);
    brdfLut = createAndUploadGpuImage2D(image, QueueFamily::Graphics, VK_IMAGE_USAGE_SAMPLED_BIT);
    freeImage(image);

    VkDescriptorImageInfo brdfLutImageInfo { nullptr, brdfLut.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    VkDescriptorImageInfo skyboxImageInfos[COUNTOF(hdriImagePaths)];
    VkDescriptorImageInfo irradianceImageInfos[COUNTOF(hdriImagePaths)];
    VkDescriptorImageInfo prefilteredImageInfos[COUNTOF(hdriImagePaths)];
    char skyboxImagePath[256], irradianceMapImagePath[256], prefilteredMapImagePath[256];

    for (uint8_t i = 0; i < COUNTOF(hdriImagePaths); i++)
    {
        const char *hdriFilename;
        size_t hdriFilenameLength;
        cwk_path_get_basename_wout_extension(hdriImagePaths[i], &hdriFilename, &hdriFilenameLength);
        snprintf(skyboxImagePath, sizeof(skyboxImagePath), "%s/%.*s_skybox.ktx2", envmapsPath, (uint32_t)hdriFilenameLength, hdriFilename);
        snprintf(irradianceMapImagePath, sizeof(irradianceMapImagePath), "%s/%.*s_irradiance.ktx2", envmapsPath, (uint32_t)hdriFilenameLength, hdriFilename);
        snprintf(prefilteredMapImagePath, sizeof(prefilteredMapImagePath), "%s/%.*s_prefiltered.ktx2", envmapsPath, (uint32_t)hdriFilenameLength, hdriFilename);

#if SKIP_ENV_MAP_REIMPORT
        if (!pathExists(skyboxImagePath))
#endif
            computeEnvMaps(hdriImagePaths[i], skyboxImagePath, irradianceMapImagePath, prefilteredMapImagePath);

        image = loadImage(skyboxImagePath);
        skyboxImages[i] = createAndUploadGpuImage2DArray(image, QueueFamily::Graphics, VK_IMAGE_USAGE_SAMPLED_BIT, Cubemap::Yes);
        freeImage(image);
        image = loadImage(irradianceMapImagePath);
        irradianceMaps[i] = createAndUploadGpuImage2DArray(image, QueueFamily::Graphics, VK_IMAGE_USAGE_SAMPLED_BIT, Cubemap::Yes);
        freeImage(image);
        image = loadImage(prefilteredMapImagePath);
        prefilteredMaps[i] = createAndUploadGpuImage2DArray(image, QueueFamily::Graphics, VK_IMAGE_USAGE_SAMPLED_BIT, Cubemap::Yes);
        freeImage(image);

        skyboxImageInfos[i] = { nullptr, skyboxImages[i].imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        irradianceImageInfos[i] = { nullptr, irradianceMaps[i].imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        prefilteredImageInfos[i] = { nullptr, prefilteredMaps[i].imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    }

    VkWriteDescriptorSet writes[]
    {
        initWriteDescriptorSetImage(globalDescriptorSet, 7, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &brdfLutImageInfo),
        initWriteDescriptorSetImage(globalDescriptorSet, 8, COUNTOF(irradianceImageInfos), VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, irradianceImageInfos),
        initWriteDescriptorSetImage(globalDescriptorSet, 9, COUNTOF(prefilteredImageInfos), VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, prefilteredImageInfos),
        initWriteDescriptorSetBuffer(skyboxDescriptorSet, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, skyboxBufferInfos),
        initWriteDescriptorSetImage(skyboxDescriptorSet, 1, COUNTOF(skyboxImageInfos), VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, skyboxImageInfos),
    };

    vkUpdateDescriptorSets(device, COUNTOF(writes), writes, 0, nullptr);
}

void loadLights()
{
    ZoneScoped;
    lightDirsAndIntensitiesUI[0] = glm::vec4(1.f, -1.f, -1.f, 0.5f);
    lightDirsAndIntensitiesUI[1] = glm::vec4(-1.f, -1.f, -1.f, 0.5f);
    lightDirsAndIntensitiesUI[2] = glm::vec4(0, -1.f, 1.f, 0.8f);

    lightData = {};
    lightData.dirLightCount = 3;
    lightData.lights[0].r = 1.f;
    lightData.lights[0].g = 1.f;
    lightData.lights[0].b = 1.f;
    lightData.lights[0].x = meshopt_quantizeHalf(lightDirsAndIntensitiesUI[0].x);
    lightData.lights[0].y = meshopt_quantizeHalf(lightDirsAndIntensitiesUI[0].y);
    lightData.lights[0].z = meshopt_quantizeHalf(lightDirsAndIntensitiesUI[0].z);
    lightData.lights[0].intensity = meshopt_quantizeHalf(lightDirsAndIntensitiesUI[0].w);
    lightData.lights[1] = lightData.lights[0];
    lightData.lights[1].x = meshopt_quantizeHalf(lightDirsAndIntensitiesUI[1].x);
    lightData.lights[1].y = meshopt_quantizeHalf(lightDirsAndIntensitiesUI[1].y);
    lightData.lights[1].z = meshopt_quantizeHalf(lightDirsAndIntensitiesUI[1].z);
    lightData.lights[1].intensity = meshopt_quantizeHalf(lightDirsAndIntensitiesUI[1].w);
    lightData.lights[2] = lightData.lights[0];
    lightData.lights[2].x = meshopt_quantizeHalf(lightDirsAndIntensitiesUI[2].x);
    lightData.lights[2].y = meshopt_quantizeHalf(lightDirsAndIntensitiesUI[2].y);
    lightData.lights[2].z = meshopt_quantizeHalf(lightDirsAndIntensitiesUI[2].z);
    lightData.lights[2].intensity = meshopt_quantizeHalf(lightDirsAndIntensitiesUI[2].w);

    uint32_t uboAlignment = (uint32_t)physicalDeviceProperties.limits.minUniformBufferOffsetAlignment;
    uint32_t bufferSize = aligned(sizeof(LightingData), uboAlignment) + aligned(sizeof(CameraData), uboAlignment) * maxFramesInFlight;

    globalUniformBuffer = createGpuBuffer(bufferSize,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);

    VkDescriptorBufferInfo bufferInfos[]
    {
        {globalUniformBuffer.buffer, 0, sizeof(LightingData)},
        {globalUniformBuffer.buffer, aligned(sizeof(LightingData), uboAlignment), aligned(sizeof(CameraData), uboAlignment)}
    };

    VkWriteDescriptorSet writes[]
    {
        initWriteDescriptorSetBuffer(globalDescriptorSet, 5, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, bufferInfos + 0),
        initWriteDescriptorSetBuffer(globalDescriptorSet, 6, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, bufferInfos + 1),
    };
    vkUpdateDescriptorSets(device, COUNTOF(writes), writes, 0, nullptr);
}

void initScene()
{
    ZoneScoped;
    prepareAssets();

    loadModel(sceneInfos[selectedScene].sceneDirPath);
    loadEnvMaps(sceneInfos[0].sceneDirPath);
    loadLights();

    camera.getPosition() = glm::vec3(0.f, 0.f, 5.f);
    //camera.lookAt(glm::vec3(scene.meshMatrices[0][3]));
}

void terminateScene()
{
    destroyGpuBuffer(modelBuffer);
    destroyGpuBuffer(skyboxBuffer);
    destroyGpuBuffer(globalUniformBuffer);

    for (uint8_t i = 0; i < modelTextureCount; i++)
    {
        destroyGpuImage(modelTextures[i]);
    }

    for (uint8_t i = 0; i < COUNTOF(hdriImagePaths); i++)
    {
        destroyGpuImage(skyboxImages[i]);
        destroyGpuImage(irradianceMaps[i]);
        destroyGpuImage(prefilteredMaps[i]);
    }

    destroyGpuImage(brdfLut);
}

void glfwKeyCallback(GLFWwindow *wnd, int key, int scancode, int action, int mods)
{
    UNUSED(wnd);
    UNUSED(scancode);
    UNUSED(mods);

    if (ImGui::GetIO().WantCaptureKeyboard)
        return;

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        if (cursorCaptured)
        {
            cursorCaptured = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            camera.onRotate(0.f, 0.f);
        }
    }
}

static void glfwMouseButtonCallback(GLFWwindow *wnd, int button, int action, int mods)
{
    UNUSED(wnd);
    UNUSED(mods);

    if (ImGui::GetIO().WantCaptureMouse)
        return;

    if (action == GLFW_PRESS)
    {
        if (button == GLFW_MOUSE_BUTTON_1)
        {
            if (!cursorCaptured)
            {
                cursorCaptured = true;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            }
        }

        if (button == GLFW_MOUSE_BUTTON_2)
        {
        }
    }
}

void compileShaders()
{
    ZoneScoped;
    ASSERT(pathExists(assetsPath) && "Assets must reside in the working dir!");

    initShaderCompiler();
    for (uint8_t i = 0; i < COUNTOF(shaderInfos); i++)
    {
        compileShaderIntoSpv(shaderInfos[i].shaderSourcePath, shaderInfos[i].shaderSpvPath, shaderInfos[i].shaderType);
    }
    terminateShaderCompiler();
}

void init()
{
    ZoneScoped;
    VERIFY(glfwInit());
    const GLFWvidmode *videoMode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    windowExtent = { (uint32_t)(0.75f * videoMode->width), (uint32_t)(0.75f * videoMode->height) };
    uiScale = videoMode->height / 1080.f;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(windowExtent.width, windowExtent.height, appName, nullptr, nullptr);
    ASSERT(window);
    glfwSetKeyCallback(window, glfwKeyCallback);
    glfwSetMouseButtonCallback(window, glfwMouseButtonCallback);

    initVulkan();
    compileShaders();
    initGraphics();
    initRenderTargets();
    initDescriptors();
    initPipelines();
    initFrameData();
    initImgui();
    initRenderdoc();
    initJobSystem();
    initImageUtils(shaderInfos[4].shaderSpvPath, shaderInfos[5].shaderSpvPath, shaderInfos[6].shaderSpvPath, shaderInfos[7].shaderSpvPath);

    initScene();
}

void exit()
{
    vkDeviceWaitIdle(device);

    terminateScene();

    terminateImageUtils();
    terminateJobSystem();
    terminateImgui();
    terminateFrameData();
    terminatePipelines();
    terminateDescriptors();
    terminateRenderTargets();
    terminateGraphics();
    terminateVulkan();

    glfwDestroyWindow(window);
    glfwTerminate();
}

void updateInput()
{
    ZoneScoped;
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    glm::vec2 cursorPos = glm::vec2((float)x, (float)y);

    if (firstCursorUpdate)
    {
        prevCursorPos = cursorPos;
        firstCursorUpdate = false;
    }

    glm::vec2 delta = cursorPos - prevCursorPos;
    prevCursorPos = cursorPos;

    if (cursorCaptured)
        camera.onRotate(delta.x, delta.y);

    float dx = (float)((glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) - (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS));
    float dy = (float)((glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) - (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS));
    float dz = (float)((glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) - (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS));

    camera.onMove(dx, dy, dz);
}

void updateLogic(float delta)
{
    ZoneScoped;
    camera.update(delta);

    sceneRotationTime += delta * rotateScene;
    glm::mat4 sceneMat = glm::eulerAngleY(glm::half_pi<float>() * sceneRotationTime);
    float aspectRatio = (float)swapchainImageExtent.width / swapchainImageExtent.height;
    glm::mat4 projMat = glm::perspective(glm::radians(45.0f), aspectRatio, 1000.f, 0.1f);

    FrameData &frame = frames[frameIndex];
    frame.cameraData.sceneMat = sceneMat;
    frame.cameraData.viewMat = camera.getViewMatrix();
    frame.cameraData.invViewMat = camera.getWorldMatrix();
    frame.cameraData.projMat = projMat;
    frame.cameraData.sceneConfig = sceneConfig;
}

void update(float delta)
{
    updateInput();
    updateLogic(delta);
}

void drawModel(VkCommandBuffer cmd)
{
    ZoneScoped;
    TracyVkZone(tracyContext, cmd, "Model");
    FrameData &frame = frames[frameIndex];
    uint32_t uboAlignment = (uint32_t)physicalDeviceProperties.limits.minUniformBufferOffsetAlignment;
    uint32_t cameraDataStaticOffset = aligned(sizeof(LightingData), uboAlignment);
    uint32_t cameraDataDynamicOffset = aligned(sizeof(CameraData), uboAlignment) * frameIndex;

    memcpy(globalUniformBuffer.mappedData, &lightData, sizeof(LightingData));
    memcpy((char *)globalUniformBuffer.mappedData + cameraDataStaticOffset + cameraDataDynamicOffset, &frame.cameraData, sizeof(CameraData));

    uint32_t pushData[]
    {
        selectedMaterial,
        selectedSkybox,
#ifdef DEBUG
        debugFlags
#endif // DEBUG
    };
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, modelPipeline);
    vkCmdPushConstants(cmd, modelPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushData), pushData);

    VkViewport viewport {};
    viewport.x = 0;
    viewport.y = (float)swapchainImageExtent.height;
    viewport.width = (float)swapchainImageExtent.width;
    viewport.height = -(float)swapchainImageExtent.height;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = swapchainImageExtent.width;
    scissor.extent.height = swapchainImageExtent.height;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    VkDescriptorSet descriptorSets[] { globalDescriptorSet, texturesDescriptorSet };
    uint32_t dynamicOffsets[] { cameraDataDynamicOffset };
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, modelPipelineLayout, 0, COUNTOF(descriptorSets), descriptorSets, COUNTOF(dynamicOffsets), dynamicOffsets);
    vkCmdDraw(cmd, (uint32_t)model.indices.size(), 1, 0, 0);
}

void drawSkybox(VkCommandBuffer cmd)
{
    ZoneScoped;
    TracyVkZone(tracyContext, cmd, "Skybox");
    uint32_t uboAlignment = (uint32_t)physicalDeviceProperties.limits.minUniformBufferOffsetAlignment;
    uint32_t cameraDataDynamicOffset = aligned(sizeof(CameraData), uboAlignment) * frameIndex;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipeline);
    vkCmdBindIndexBuffer(cmd, skyboxBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdPushConstants(cmd, modelPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(uint32_t), &selectedSkybox);
    VkDescriptorSet descriptorSets[] { globalDescriptorSet, skyboxDescriptorSet };
    uint32_t dynamicOffsets[] { cameraDataDynamicOffset };
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipelineLayout, 0, COUNTOF(descriptorSets), descriptorSets, COUNTOF(dynamicOffsets), dynamicOffsets);
    vkCmdDrawIndexed(cmd, (uint32_t)skybox.indices.size(), 1, 0, 0, 0);
}

void drawUI(VkCommandBuffer cmd)
{
    ZoneScoped;
    TracyVkZone(tracyContext, cmd, "ImGui");
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ////////
    //ImGui::ShowDemoWindow();
    ImGui::Begin(appName, nullptr, ImGuiWindowFlags_AlwaysAutoResize);

    if (ImGui::BeginTable("Table", 2, ImGuiTableFlags_NoHostExtendX))
    {
        const char *basename;
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Model");
        ImGui::TableNextColumn();
        cwk_path_get_basename(sceneInfos[selectedScene].sceneDirPath, &basename, nullptr);
        if (ImGui::BeginCombo("##Model", basename, ImGuiComboFlags_WidthFitPreview))
        {
            for (uint8_t i = 0; i < COUNTOF(sceneInfos); i++)
            {
                bool selected = i == selectedScene;
                cwk_path_get_basename(sceneInfos[i].sceneDirPath, &basename, nullptr);
                if (ImGui::Selectable(basename, selected) && !selected)
                {
                    selectedScene = i;
                    sceneChangeRequired = true;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Material");
        ImGui::TableNextColumn();
        if (ImGui::BeginCombo("##Material", materialNames[selectedMaterial], ImGuiComboFlags_WidthFitPreview))
        {
            for (uint8_t i = 0; i < COUNTOF(materialNames); i++)
            {
                bool selected = i == selectedMaterial;
                if (ImGui::Selectable(materialNames[i], selected) && !selected)
                {
                    selectedMaterial = i;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Skybox");
        ImGui::TableNextColumn();
        cwk_path_get_basename(hdriImagePaths[selectedSkybox], &basename, nullptr);
        if (ImGui::BeginCombo("##Skybox", basename, ImGuiComboFlags_WidthFitPreview))
        {
            for (uint8_t i = 0; i < COUNTOF(hdriImagePaths); i++)
            {
                bool selected = i == selectedSkybox;
                cwk_path_get_basename(hdriImagePaths[i], &basename, nullptr);
                if (ImGui::Selectable(basename, selected) && !selected)
                {
                    selectedSkybox = i;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Rotate model");
        ImGui::TableNextColumn();
        ImGui::Checkbox("##Rotate model", &rotateScene);

        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Show wireframe");
        ImGui::TableNextColumn();
        ImGui::CheckboxFlags("##Show wireframe", &sceneConfig, SCENE_SHOW_WIREFRAME);

        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Use normal map");
        ImGui::TableNextColumn();
        ImGui::CheckboxFlags("##Use normal map", &sceneConfig, SCENE_USE_NORMAL_MAP);

        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Use lights");
        ImGui::TableNextColumn();
        ImGui::CheckboxFlags("##Use lights", &sceneConfig, SCENE_USE_LIGHTS);

        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Use IBL");
        ImGui::TableNextColumn();
        ImGui::CheckboxFlags("##Use IBL", &sceneConfig, SCENE_USE_IBL);

#ifdef DEBUG
        static const uint32_t debugChannels[]
        {
            0,
            DEBUG_SHOW_COLOR,
            DEBUG_SHOW_NORMAL,
            DEBUG_SHOW_AO,
            DEBUG_SHOW_ROUGHNESS,
            DEBUG_SHOW_METALLIC,
            DEBUG_SHOW_IRRADIANCE,
            DEBUG_SHOW_PREFILTERED,
            DEBUG_SHOW_DIFFUSE,
            DEBUG_SHOW_SPECULAR
        };
        static const char *const debugChannelNames[] { "None", "Color", "Normal", "AO", "Roughness", "Metallic", "Irradiance", "Prefiltered", "Diffuse", "Specular" };
        static uint8_t selectedChannel = 0;
        static_assert(COUNTOF(debugChannels) == COUNTOF(debugChannelNames), "");

        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Debug Channel");
        ImGui::TableNextColumn();
        if (ImGui::BeginCombo("##Debug Channel", debugChannelNames[selectedChannel], ImGuiComboFlags_WidthFitPreview))
        {
            for (uint8_t i = 0; i < COUNTOF(debugChannelNames); i++)
            {
                bool selected = i == selectedChannel;
                if (ImGui::Selectable(debugChannelNames[i], selected) && !selected)
                {
                    selectedChannel = i;
                    debugFlags = debugChannels[selectedChannel];
                }
            }
            ImGui::EndCombo();
        }
#endif // DEBUG

        ImGui::EndTable();
    }

    if (ImGui::CollapsingHeader("Lights"))
    {
        if (ImGui::TreeNode("Dir lights"))
        {
            if (ImGui::BeginTable("##Table", 3, ImGuiTableFlags_NoHostExtendX | ImGuiTableFlags_Borders))
            {
                ImGui::TableSetupColumn("#");
                ImGui::TableSetupColumn("Direction");
                ImGui::TableSetupColumn("Intensity");
                ImGui::TableHeadersRow();

                for (uint32_t i = 0; i < lightData.dirLightCount; i++)
                {
                    ImGui::PushID(i);
                    ImGui::TableNextColumn();
                    ImGui::AlignTextToFramePadding();
                    ImGui::Text("%2d", i);
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(15 * ImGui::GetFontSize());
                    if (ImGui::SliderFloat3("", (float *)&lightDirsAndIntensitiesUI[i], -1.f, 1.f, "%.2f", ImGuiSliderFlags_AlwaysClamp))
                    {
                        lightData.lights[i].x = meshopt_quantizeHalf(lightDirsAndIntensitiesUI[i].x);
                        lightData.lights[i].y = meshopt_quantizeHalf(lightDirsAndIntensitiesUI[i].y);
                        lightData.lights[i].z = meshopt_quantizeHalf(lightDirsAndIntensitiesUI[i].z);
                    }
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(5 * ImGui::GetFontSize());
                    if (ImGui::SliderFloat("", (float *)&lightDirsAndIntensitiesUI[i] + 3, 0.f, 10.f, "%.2f", ImGuiSliderFlags_AlwaysClamp))
                    {
                        lightData.lights[i].intensity = meshopt_quantizeHalf(lightDirsAndIntensitiesUI[i].w);
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }
    }

    ImGui::End();
    ////////
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

void draw()
{
    ZoneScoped;
    if (resizeNeeded)
    {
        recreateRenderTargets();
        resizeNeeded = false;
    }

    FrameData &frame = frames[frameIndex];
    vkVerify(vkWaitForFences(device, 1, &frame.renderFinishedFence, true, UINT64_MAX));
    uint32_t swapchainImageIndex;
    VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, frame.imageAcquiredSemaphore, nullptr, &swapchainImageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        // dummy submit to wait for the semaphore
        VkSemaphoreSubmitInfoKHR waitSemaphoreSubmitInfo = initSemaphoreSubmitInfo(frame.imageAcquiredSemaphore, VK_PIPELINE_STAGE_2_NONE_KHR);
        VkSubmitInfo2 submitInfo = initSubmitInfo(nullptr, &waitSemaphoreSubmitInfo, nullptr);
        vkVerify(vkQueueSubmit2KHR(graphicsQueue, 1, &submitInfo, nullptr));
        resizeNeeded = true;
        return;
    }
    else
    {
        vkVerify(result);
    }

    vkVerify(vkResetFences(device, 1, &frame.renderFinishedFence));

    vkVerify(vkResetCommandPool(device, frame.commandPool, 0));
    VkCommandBuffer cmd = frame.commandBuffer;
    VkCommandBufferBeginInfo commandBufferBeginInfo = initCommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    vkVerify(vkBeginCommandBuffer(cmd, &commandBufferBeginInfo));
    {
        TracyVkZone(tracyContext, cmd, "Draw");

        VkClearValue colorClearValue {}, depthClearValue {};
        VkRenderingAttachmentInfoKHR colorAttachmentInfo = initRenderingAttachmentInfo(swapchainImageViews[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, &colorClearValue);
        VkRenderingAttachmentInfoKHR depthAttachmentInfo = initRenderingAttachmentInfo(depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, &depthClearValue);
        VkRenderingInfoKHR renderingInfo = initRenderingInfo(swapchainImageExtent, &colorAttachmentInfo, 1, &depthAttachmentInfo);

        ImageBarrier imageBarrier {};
        imageBarrier.image = swapchainImages[swapchainImageIndex];
        imageBarrier.srcStageMask = StageFlags::ColorAttachment; // wait for acquire semaphore
        imageBarrier.dstStageMask = StageFlags::ColorAttachment;
        imageBarrier.srcAccessMask = AccessFlags::None;
        imageBarrier.dstAccessMask = AccessFlags::Write;
        imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        pipelineBarrier(cmd, nullptr, 0, &imageBarrier, 1);

        vkCmdBeginRenderingKHR(cmd, &renderingInfo);
        drawModel(cmd);
        drawSkybox(cmd);
        drawUI(cmd);
        vkCmdEndRenderingKHR(cmd);

        imageBarrier.srcStageMask = StageFlags::ColorAttachment;
        imageBarrier.dstStageMask = StageFlags::ColorAttachment; // make render semaphore wait
        imageBarrier.srcAccessMask = AccessFlags::Write;
        imageBarrier.dstAccessMask = AccessFlags::None;
        imageBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        imageBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        pipelineBarrier(cmd, nullptr, 0, &imageBarrier, 1);
    }
    TracyVkCollect(tracyContext, cmd);
    vkVerify(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfoKHR cmdSubmitInfo = initCommandBufferSubmitInfo(cmd);
    VkSemaphoreSubmitInfoKHR waitSemaphoreSubmitInfo = initSemaphoreSubmitInfo(frame.imageAcquiredSemaphore, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR);
    VkSemaphoreSubmitInfoKHR signalSemaphoreSubmitInfo = initSemaphoreSubmitInfo(frame.renderFinishedSemaphore, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR);
    VkSubmitInfo2 submitInfo = initSubmitInfo(&cmdSubmitInfo, &waitSemaphoreSubmitInfo, &signalSemaphoreSubmitInfo);
    vkVerify(vkQueueSubmit2KHR(graphicsQueue, 1, &submitInfo, frame.renderFinishedFence));

    VkPresentInfoKHR presentInfo = initPresentInfo(&swapchain, &frame.renderFinishedSemaphore, &swapchainImageIndex);
    result = vkQueuePresentKHR(graphicsQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        resizeNeeded = true;
    }
    else
    {
        vkVerify(result);
    }

    if (sceneChangeRequired)
    {
        vkDeviceWaitIdle(device);
        loadModel(sceneInfos[selectedScene].sceneDirPath);
        sceneChangeRequired = false;
    }

    frameIndex = (frameIndex + 1) % maxFramesInFlight;
}