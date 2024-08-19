#include <cwalk.h>
#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <meshoptimizer.h>
#include <VkBootstrap.h>

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
float timeSinceStart = 0.f;

void init();
void update(float delta);
void draw();
void exit();

int main()
{
    init();

    const float defaultDelta = 1.f / 60.f;
    float delta = defaultDelta;
    uint64_t startTime = glfwGetTimerValue();
    uint64_t prevTime = startTime;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        update(delta);
        draw();

        uint64_t currTime = glfwGetTimerValue();
        float period = 1.f / glfwGetTimerFrequency();
        timeSinceStart = (currTime - startTime) * period;
        delta = (currTime - prevTime) * period;
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

struct ShaderCompileInfo
{
    const char *shaderSourcePath;
    const char *shaderSpvPath;
    ShaderType shaderType;
    uint8_t pad[3];
};

#define defineShader(name, ext, type) ShaderCompileInfo name##type##Shader{shadersPath #name ext, spvsPath #name ext ".spv", ShaderType::type}

struct ShaderTable
{
    defineShader(renderModel, ".vert", Vertex);
    defineShader(renderModel, ".frag", Fragment);
    defineShader(renderSkybox, ".vert", Vertex);
    defineShader(renderSkybox, ".frag", Fragment);
    defineShader(renderCutLine, ".vert", Vertex);
    defineShader(renderCutLine, ".frag", Fragment);
    defineShader(renderBurnMap, ".vert", Vertex);
    defineShader(renderBurnMap, ".frag", Fragment);
    defineShader(computeSkybox, ".comp", Compute);
    defineShader(computeBrdfLut, ".comp", Compute);
    defineShader(computeIrradianceMap, ".comp", Compute);
    defineShader(computePrefilteredMap, ".comp", Compute);
    defineShader(normalizeNormalMap, ".comp", Compute);
    defineShader(computePlaneCut, ".comp", Compute);
    defineShader(computeBlur2D, ".comp", Compute);
    defineShader(computeBloomAndTonemap, ".comp", Compute);
} shaderTable;

const struct SceneImportInfo
{
    const char *glbAssetPath;
    const char *sceneDirPath;
    float scalingFactor;
} sceneInfos[]
{
    {glbsPath"Cube.glb",              scenesPath"Cube",              1.f},
    {glbsPath"ShaderBall.glb",        scenesPath"ShaderBall",        1.f},
    {glbsPath"Lantern.glb",           scenesPath"Lantern",           0.1f},
    {glbsPath"WaterBottle.glb",       scenesPath"WaterBottle",       8.f},
    {glbsPath"SciFiHelmet.glb",       scenesPath"SciFiHelmet",       1.f},
    {glbsPath"NormalTangentTest.glb", scenesPath"NormalTangentTest", 1.f}
};

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
static_assert(countOf(materialInfos) + 1 == countOf(materialNames), "");

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
std::vector<VkImage> swapchainImages;
GpuImage frameBufferImage;
std::vector<VkImageView> bloomImageMipImageViews;
std::vector<glm::uvec2> bloomImageMipSizes;
GpuImage bloomImage;
GpuImage resolveImage;
GpuImage depthImage;

VkDescriptorPool descriptorPool;
VkDescriptorSetLayout globalDescriptorSetLayout;
VkDescriptorSet globalDescriptorSet;

VkPipelineLayout graphicsPipelineLayout;
VkPipelineLayout computePipelineLayout;
VkPipeline modelPipeline;
VkPipeline skyboxPipeline;
VkPipeline linePipeline;
VkPipeline cuttingPipeline;
VkPipeline burnMapPipeline;
VkPipeline blurPipeline;
VkPipeline bloomAndTonemapPipeline;

GpuBuffer modelBuffer;
GpuBuffer globalUniformBuffer;
GpuBuffer drawIndirectBuffer;

GpuImage modelTextures[MAX_MODEL_TEXTURES];
uint8_t modelTextureCount;
GpuImage skyboxImages[countOf(hdriImagePaths)];
GpuImage irradianceMaps[countOf(hdriImagePaths)];
GpuImage prefilteredMaps[countOf(hdriImagePaths)];
GpuImage brdfLut;
GpuImage burnMapImage;

VkSampler linearClampSampler;
VkSampler linearRepeatSampler;
VkSampler nearestClampSampler;
VkSampler nearestRepeatSampler;

Cmd graphicsCmd;
Cmd computeCmd;
VkFence computeFence;

struct FrameData
{
    VkSemaphore imageAcquiredSemaphore, renderFinishedSemaphore;
    VkFence renderFinishedFence;
    Cmd cmd;
    SceneData sceneData;
};

const uint8_t maxFramesInFlight = 2;
uint8_t frameIndex = 0;
FrameData frames[maxFramesInFlight];

FlyCamera camera;
Scene model;
LightingData lightData;
DrawIndirectData drawIndirectReadData;
uint8_t drawDataReadIndex = 0; // 0 or 1
LineData lineData;
CuttingData cuttingData;

enum class CutState : uint8_t
{
    None = 0,
    CutLineStarted,
    CutLineFinished,
    CuttingInProgress
} cutState;

const float defaultLineWidth = 30.f;

uint8_t selectedScene = 1;
uint32_t selectedMaterial = 0;
uint32_t selectedSkybox = 1;
uint32_t maxMsaaSampleCount = 0;
uint32_t msaaSampleCount = 2;
float bloomStrength = 0.02f;

bool rotateScene = false;
float sceneRotationTime = 0.f;
uint32_t sceneConfig = SCENE_USE_LIGHTS | SCENE_USE_IBL;
glm::vec4 lightDirsAndIntensitiesUI[MAX_LIGHTS];

glm::vec2 prevCursorPos;
bool firstCursorUpdate = true;
bool cursorCaptured = false;

bool renderTargetsChangeRequired = false;
bool sceneChangeRequired = false;
bool shaderReloadRequired = false;
bool imguiVulkanResetRequired = false;
bool lastShaderReloadSuccessful = true;
bool vSyncOn = true;

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
    features.shaderInt16 = true;
    features.shaderSampledImageArrayDynamicIndexing = true;
    features.textureCompressionBC = true;
    features.shaderStorageImageMultisample = true;
    VkPhysicalDeviceVulkan11Features features11 {};
    features11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    features11.storageBuffer16BitAccess = true;
    features11.uniformAndStorageBuffer16BitAccess = true;
    VkPhysicalDeviceVulkan12Features features12 {};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.shaderFloat16 = true;
    features12.shaderInt8 = true;
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
        .set_desired_present_mode(vSyncOn ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR)
        .set_desired_extent(width, height)
        .set_desired_min_image_count(3)
        .set_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .build()
        .value();

    swapchain = vkbSwapchain.swapchain;
    swapchainImages = vkbSwapchain.get_images().value();
}

void destroySwapchain()
{
    vkDestroySwapchainKHR(device, swapchain, nullptr);
}

void initRenderTargets()
{
    ZoneScoped;
    createSwapchain(windowExtent.width, windowExtent.height);

    VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;
    VkImageUsageFlags usageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    VkImageFormatProperties formatProperties;
    vkGetPhysicalDeviceImageFormatProperties(physicalDevice, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, usageFlags, 0, &formatProperties);
    maxMsaaSampleCount = formatProperties.sampleCounts;
    vkGetPhysicalDeviceImageFormatProperties(physicalDevice, VK_FORMAT_D32_SFLOAT, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 0, &formatProperties);
    maxMsaaSampleCount &= formatProperties.sampleCounts; // yes, this isn't max actually

    // mip 0 for rendering, mips 1-n for bloom bluring
    frameBufferImage = createGpuImage(format, windowExtent, 1, usageFlags, GpuImageType::Image2D, VK_IMAGE_ASPECT_COLOR_BIT, msaaSampleCount);
    setGpuImageName(frameBufferImage, NAMEOF(frameBufferImage));

    bloomImage = createGpuImage(VK_FORMAT_R16G16B16A16_SFLOAT, windowExtent, 6,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        GpuImageType::Image2D, VK_IMAGE_ASPECT_COLOR_BIT, 1);
    setGpuImageName(bloomImage, NAMEOF(bloomImage));

    resolveImage = createGpuImage(VK_FORMAT_R16G16B16A16_SFLOAT, windowExtent, 1,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        GpuImageType::Image2D, VK_IMAGE_ASPECT_COLOR_BIT, 1);
    setGpuImageName(resolveImage, NAMEOF(resolveImage));

    depthImage = createGpuImage(VK_FORMAT_D32_SFLOAT, windowExtent, 1,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        GpuImageType::Image2D, VK_IMAGE_ASPECT_DEPTH_BIT, msaaSampleCount);
    setGpuImageName(depthImage, NAMEOF(depthImage));

    bloomImageMipImageViews.resize(bloomImage.levelCount);
    bloomImageMipSizes.resize(bloomImage.levelCount);
    uint32_t width = windowExtent.width, height = windowExtent.height;

    for (uint8_t i = 0; i < bloomImage.levelCount; i++)
    {
        VkImageSubresourceRange range = initImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, i, 1);
        VkImageViewCreateInfo imageViewCreateInfo = initImageViewCreateInfo(bloomImage.image, bloomImage.format, VK_IMAGE_VIEW_TYPE_2D, range);
        vkVerify(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &bloomImageMipImageViews[i]));
        bloomImageMipSizes[i] = glm::uvec2(width, height);

        if (width)
            width >>= 1;
        if (height)
            height >>= 1;
    }
}

void terminateRenderTargets()
{
    destroySwapchain();
    destroyGpuImage(frameBufferImage);
    destroyGpuImage(bloomImage);
    destroyGpuImage(resolveImage);
    destroyGpuImage(depthImage);

    for (VkImageView view : bloomImageMipImageViews)
    {
        vkDestroyImageView(device, view, nullptr);
    }
    bloomImageMipImageViews.clear();
    bloomImageMipSizes.clear();
}

void updateRenderTargetDescriptors()
{
    uint32_t size = (1 + bloomImage.levelCount * 2) * sizeof(VkDescriptorImageInfo);
    VkDescriptorImageInfo *imageInfos = (VkDescriptorImageInfo *)alloca(size);
    memset(imageInfos, 0, size);

    imageInfos[0] = { nullptr, frameBufferImage.imageView, VK_IMAGE_LAYOUT_GENERAL };

    for (uint8_t i = 0; i < bloomImage.levelCount; i++)
    {
        imageInfos[1 + i] = { nullptr, bloomImageMipImageViews[i], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        imageInfos[1 + i + bloomImage.levelCount] = { nullptr, bloomImageMipImageViews[i], VK_IMAGE_LAYOUT_GENERAL };
    }

    VkWriteDescriptorSet writes[]
    {
        initWriteDescriptorSetImage(globalDescriptorSet, 25, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, imageInfos),
        initWriteDescriptorSetImage(globalDescriptorSet, 26, bloomImage.levelCount, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, imageInfos + 1),
        initWriteDescriptorSetImage(globalDescriptorSet, 27, bloomImage.levelCount, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, imageInfos + 1 + bloomImage.levelCount)
    };
    vkUpdateDescriptorSets(device, countOf(writes), writes, 0, nullptr);
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
    updateRenderTargetDescriptors();
}

void initFrameData()
{
    ZoneScoped;
    graphicsCmd = allocateCmd(QueueFamily::Graphics);
    computeCmd = allocateCmd(QueueFamily::Compute);
    VkFenceCreateInfo fenceCreateInfo = initFenceCreateInfo();
    vkVerify(vkCreateFence(device, &fenceCreateInfo, nullptr, &computeFence));
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    VkSemaphoreCreateInfo semaphoreCreateInfo = initSemaphoreCreateInfo();

    for (FrameData &frame : frames)
    {
        frame.cmd = allocateCmd(QueueFamily::Graphics);

        vkVerify(vkCreateFence(device, &fenceCreateInfo, nullptr, &frame.renderFinishedFence));
        vkVerify(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &frame.imageAcquiredSemaphore));
        vkVerify(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &frame.renderFinishedSemaphore));
    }
}

void terminateFrameData()
{
    vkDestroyFence(device, computeFence, nullptr);

    for (FrameData &frame : frames)
    {
        vkDestroyFence(device, frame.renderFinishedFence, nullptr);
        vkDestroySemaphore(device, frame.imageAcquiredSemaphore, nullptr);
        vkDestroySemaphore(device, frame.renderFinishedSemaphore, nullptr);
    }
}

void initDescriptors()
{
    ZoneScoped;
    VkSamplerCreateInfo samplerCreateInfo = initSamplerCreateInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    vkVerify(vkCreateSampler(device, &samplerCreateInfo, nullptr, &linearClampSampler));
    samplerCreateInfo = initSamplerCreateInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);
    vkVerify(vkCreateSampler(device, &samplerCreateInfo, nullptr, &linearRepeatSampler));
    samplerCreateInfo = initSamplerCreateInfo(VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    vkVerify(vkCreateSampler(device, &samplerCreateInfo, nullptr, &nearestClampSampler));
    samplerCreateInfo = initSamplerCreateInfo(VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_REPEAT);
    vkVerify(vkCreateSampler(device, &samplerCreateInfo, nullptr, &nearestRepeatSampler));

    VkDescriptorPoolSize poolSizes[]
    {
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 64},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 64},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 64},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 64},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 64},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 64 + MAX_MODEL_TEXTURES},
        {VK_DESCRIPTOR_TYPE_SAMPLER, 64},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1} // for imgui
    };
    VkDescriptorPoolCreateInfo poolCreateInfo = initDescriptorPoolCreateInfo(10, poolSizes, countOf(poolSizes), VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT /* for imgui */);
    vkVerify(vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &descriptorPool));

    VkDescriptorSetLayoutBinding globalSetBindings[]
    {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT}, // draw data
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT}, // read indices
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT}, // write indices
        {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT}, // positions
        {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT}, // normalUvs
        {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT}, // transforms
        {6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT}, // materials
        {7, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT}, // lights
        {8, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT}, // camera
        {10, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, &linearClampSampler},
        {11, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, &linearRepeatSampler},
        {12, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, &nearestClampSampler},
        {13, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, &nearestRepeatSampler},
        {20, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT}, // brdf lut
        {21, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, countOf(skyboxImages), VK_SHADER_STAGE_FRAGMENT_BIT}, // skybox
        {22, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, countOf(irradianceMaps), VK_SHADER_STAGE_FRAGMENT_BIT}, // irradiance
        {23, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, countOf(prefilteredMaps), VK_SHADER_STAGE_FRAGMENT_BIT}, // prefiltered
        {24, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT}, // burn map texture
        {25, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT}, // framebuffer image
        {26, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, bloomImage.levelCount, VK_SHADER_STAGE_COMPUTE_BIT}, // frame buffer mips
        {27, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, bloomImage.levelCount, VK_SHADER_STAGE_COMPUTE_BIT}, // frame buffer mips
        {30, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MAX_MODEL_TEXTURES, VK_SHADER_STAGE_FRAGMENT_BIT } // material textures
    };

    VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo = initDescriptorSetLayoutCreateInfo(globalSetBindings, countOf(globalSetBindings));
    vkVerify(vkCreateDescriptorSetLayout(device, &setLayoutCreateInfo, nullptr, &globalDescriptorSetLayout));

    VkDescriptorSetAllocateInfo setAllocateInfo = initDescriptorSetAllocateInfo(descriptorPool, &globalDescriptorSetLayout, 1);
    vkVerify(vkAllocateDescriptorSets(device, &setAllocateInfo, &globalDescriptorSet));
}

void terminateDescriptors()
{
    vkDestroySampler(device, linearClampSampler, nullptr);
    vkDestroySampler(device, linearRepeatSampler, nullptr);
    vkDestroySampler(device, nearestClampSampler, nullptr);
    vkDestroySampler(device, nearestRepeatSampler, nullptr);
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device, globalDescriptorSetLayout, nullptr);
}

void initPipelines()
{
    ZoneScoped;
    VkPushConstantRange pushRange { VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushData) };
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = initPipelineLayoutCreateInfo(&globalDescriptorSetLayout, 1, &pushRange, 1);
    vkVerify(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &graphicsPipelineLayout));

    pushRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CuttingData) };
    vkVerify(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &computePipelineLayout));

    // Model
    VkPipelineColorBlendAttachmentState blendState {};
    blendState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkShaderModule vertexShader = createShaderModuleFromSpv(device, shaderTable.renderModelVertexShader.shaderSpvPath);
    VkShaderModule fragmentShader = createShaderModuleFromSpv(device, shaderTable.renderModelFragmentShader.shaderSpvPath);
    VkPipelineShaderStageCreateInfo stages[2];
    stages[0] = initPipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vertexShader);
    stages[1] = initPipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader);
    VkPipelineVertexInputStateCreateInfo vertexInputState = initPipelineVertexInputStateCreateInfo(nullptr, 0, nullptr, 0);
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = initPipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    VkPipelineViewportStateCreateInfo viewportState = initPipelineViewportStateCreateInfo();
    VkPipelineRasterizationStateCreateInfo rasterizationState = initPipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    VkPipelineMultisampleStateCreateInfo multisampleState = initPipelineMultisampleStateCreateInfo(msaaSampleCount);
    VkPipelineDepthStencilStateCreateInfo depthStencilState = initPipelineDepthStencilStateCreateInfo(true, true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    VkPipelineColorBlendStateCreateInfo colorBlendState = initPipelineColorBlendStateCreateInfo(&blendState, 1);
    VkPipelineDynamicStateCreateInfo dynamicState = initPipelineDynamicStateCreateInfo();
    VkPipelineRenderingCreateInfoKHR renderingInfo = initPipelineRenderingCreateInfo(&frameBufferImage.format, 1, depthImage.format);
    VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = initGraphicsPipelineCreateInfo(graphicsPipelineLayout,
        stages,
        countOf(stages),
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
    vertexShader = createShaderModuleFromSpv(device, shaderTable.renderSkyboxVertexShader.shaderSpvPath);
    fragmentShader = createShaderModuleFromSpv(device, shaderTable.renderSkyboxFragmentShader.shaderSpvPath);
    stages[0] = initPipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vertexShader);
    stages[1] = initPipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader);
    rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
    depthStencilState = initPipelineDepthStencilStateCreateInfo(true, false, VK_COMPARE_OP_GREATER_OR_EQUAL);
    vkVerify(vkCreateGraphicsPipelines(device, nullptr, 1, &graphicsPipelineCreateInfo, nullptr, &skyboxPipeline));
    vkDestroyShaderModule(device, vertexShader, nullptr);
    vkDestroyShaderModule(device, fragmentShader, nullptr);

    // Line
    vertexShader = createShaderModuleFromSpv(device, shaderTable.renderCutLineVertexShader.shaderSpvPath);
    fragmentShader = createShaderModuleFromSpv(device, shaderTable.renderCutLineFragmentShader.shaderSpvPath);
    stages[0] = initPipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vertexShader);
    stages[1] = initPipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader);
    depthStencilState = initPipelineDepthStencilStateCreateInfo(false, false, VK_COMPARE_OP_NEVER);
    blendState.blendEnable = true;
    blendState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendState.colorBlendOp = VK_BLEND_OP_ADD;
    blendState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blendState.alphaBlendOp = VK_BLEND_OP_ADD;
    vkVerify(vkCreateGraphicsPipelines(device, nullptr, 1, &graphicsPipelineCreateInfo, nullptr, &linePipeline));
    vkDestroyShaderModule(device, vertexShader, nullptr);
    vkDestroyShaderModule(device, fragmentShader, nullptr);

    // Burn
    vertexShader = createShaderModuleFromSpv(device, shaderTable.renderBurnMapVertexShader.shaderSpvPath);
    fragmentShader = createShaderModuleFromSpv(device, shaderTable.renderBurnMapFragmentShader.shaderSpvPath);
    stages[0] = initPipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vertexShader);
    stages[1] = initPipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader);
    rasterizationState.cullMode = VK_CULL_MODE_NONE;
    multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    depthStencilState = initPipelineDepthStencilStateCreateInfo(false, false, VK_COMPARE_OP_NEVER);
    blendState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    blendState.colorBlendOp = VK_BLEND_OP_ADD;
    blendState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blendState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blendState.alphaBlendOp = VK_BLEND_OP_MAX;
    VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
    renderingInfo = initPipelineRenderingCreateInfo(&format, 1);
    vkVerify(vkCreateGraphicsPipelines(device, nullptr, 1, &graphicsPipelineCreateInfo, nullptr, &burnMapPipeline));
    vkDestroyShaderModule(device, vertexShader, nullptr);
    vkDestroyShaderModule(device, fragmentShader, nullptr);

    // Cutting
    VkShaderModule computeShader = createShaderModuleFromSpv(device, shaderTable.computePlaneCutComputeShader.shaderSpvPath);
    VkPipelineShaderStageCreateInfo computeShaderStageCreateInfo = initPipelineShaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT, computeShader);
    VkComputePipelineCreateInfo computePipelineCreateInfo = initComputePipelineCreateInfo(computeShaderStageCreateInfo, computePipelineLayout);
    vkVerify(vkCreateComputePipelines(device, nullptr, 1, &computePipelineCreateInfo, nullptr, &cuttingPipeline));
    vkDestroyShaderModule(device, computeShader, nullptr);

    // Blur for bloom
    computeShader = createShaderModuleFromSpv(device, shaderTable.computeBlur2DComputeShader.shaderSpvPath);
    computeShaderStageCreateInfo = initPipelineShaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT, computeShader);
    computePipelineCreateInfo = initComputePipelineCreateInfo(computeShaderStageCreateInfo, computePipelineLayout);
    vkVerify(vkCreateComputePipelines(device, nullptr, 1, &computePipelineCreateInfo, nullptr, &blurPipeline));
    vkDestroyShaderModule(device, computeShader, nullptr);

    // Bloom and tonemap
    VkSpecializationMapEntry entry { 0, 0, sizeof(uint32_t) };
    VkSpecializationInfo specInfo { 1, &entry, sizeof(uint32_t), &msaaSampleCount };
    computeShader = createShaderModuleFromSpv(device, shaderTable.computeBloomAndTonemapComputeShader.shaderSpvPath);
    computeShaderStageCreateInfo = initPipelineShaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT, computeShader, &specInfo);
    computePipelineCreateInfo = initComputePipelineCreateInfo(computeShaderStageCreateInfo, computePipelineLayout);
    vkVerify(vkCreateComputePipelines(device, nullptr, 1, &computePipelineCreateInfo, nullptr, &bloomAndTonemapPipeline));
    vkDestroyShaderModule(device, computeShader, nullptr);
}

void terminatePipelines()
{
    vkDestroyPipelineLayout(device, graphicsPipelineLayout, nullptr);
    vkDestroyPipelineLayout(device, computePipelineLayout, nullptr);
    vkDestroyPipeline(device, modelPipeline, nullptr);
    vkDestroyPipeline(device, skyboxPipeline, nullptr);
    vkDestroyPipeline(device, cuttingPipeline, nullptr);
    vkDestroyPipeline(device, linePipeline, nullptr);
    vkDestroyPipeline(device, burnMapPipeline, nullptr);
    vkDestroyPipeline(device, blurPipeline, nullptr);
    vkDestroyPipeline(device, bloomAndTonemapPipeline, nullptr);
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

void initImguiVulkan()
{
    ImGui_ImplVulkan_InitInfo imguiInitInfo {};
    imguiInitInfo.Instance = instance;
    imguiInitInfo.PhysicalDevice = physicalDevice;
    imguiInitInfo.Device = device;
    imguiInitInfo.QueueFamily = graphicsQueueFamilyIndex;
    imguiInitInfo.Queue = graphicsQueue;
    imguiInitInfo.DescriptorPool = descriptorPool;
    imguiInitInfo.MinImageCount = maxFramesInFlight;
    imguiInitInfo.ImageCount = maxFramesInFlight;
    imguiInitInfo.MSAASamples = (VkSampleCountFlagBits)msaaSampleCount;
    imguiInitInfo.UseDynamicRendering = true;
    imguiInitInfo.PipelineRenderingCreateInfo = initPipelineRenderingCreateInfo(&frameBufferImage.format, 1, depthImage.format);
    ImGui_ImplVulkan_Init(&imguiInitInfo);
}

void terminateImguiVulkan()
{
    ImGui_ImplVulkan_Shutdown();
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
    initImguiVulkan();

    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr; // disable imgui.ini
    io.Fonts->AddFontFromFileTTF(defaultFontPath, defaultFontSize * uiScale);
    ImGui::GetStyle().ScaleAllSizes(uiScale);
}

void terminateImgui()
{
    terminateImguiVulkan();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void prepareAssets()
{
    ZoneScoped;
    ASSERT(pathExists(assetsPath) && "Assets must reside in the working dir!");

    for (uint8_t i = 0; i < countOf(sceneInfos); i++)
    {
#if SKIP_SCENE_REIMPORT
        if (!pathExists(sceneInfos[i].sceneDirPath))
#endif
            importSceneFromGlb(sceneInfos[i].glbAssetPath, sceneInfos[i].sceneDirPath, sceneInfos[i].scalingFactor);
    }

    for (uint8_t i = 0; i < countOf(materialInfos); i++)
    {
#if SKIP_MATERIAL_REIMPORT
        if (!pathExists(materialInfos[i].dstSet.baseColorTexPath))
#endif
            importMaterial(materialInfos[i].srcSet, materialInfos[i].dstSet);
    }
}

static const uint32_t maxIndexCount = UINT16_MAX * 32;
static const uint32_t maxVertexCount = UINT16_MAX * 4;
static const uint32_t maxTransformCount = UINT8_MAX;
static const uint32_t maxMaterialCount = UINT8_MAX;
static const uint32_t maxIndicesSize = maxIndexCount * sizeof(uint32_t);
static const uint32_t maxPositionsSize = maxVertexCount * sizeof(Position);
static const uint32_t maxNormalUvsSize = maxVertexCount * sizeof(NormalUv);
static const uint32_t maxTransformsSize = maxTransformCount * sizeof(TransformData);
static const uint32_t maxMaterialsSize = maxMaterialCount * sizeof(MaterialData);

void loadModel(const char *sceneDirPath)
{
    ZoneScoped;
    uint32_t sboAlignment = (uint32_t)physicalDeviceProperties.limits.minStorageBufferOffsetAlignment;
    uint32_t maxReadIndicesOffset = 0;
    uint32_t maxWriteIndicesOffset = aligned(maxReadIndicesOffset + maxIndicesSize, sboAlignment);
    uint32_t maxPositionsOffset = aligned(maxWriteIndicesOffset + maxIndicesSize, sboAlignment);
    uint32_t maxNormalUvsOffset = aligned(maxPositionsOffset + maxPositionsSize, sboAlignment);
    uint32_t maxTransformsOffset = aligned(maxNormalUvsOffset + maxNormalUvsSize, sboAlignment);
    uint32_t maxMaterialsOffset = aligned(maxTransformsOffset + maxTransformsSize, sboAlignment);

    if (!modelBuffer.buffer)
    {
        uint32_t maxBufferSize = maxMaterialsOffset + maxMaterialsSize;
        modelBuffer = createGpuBuffer(maxBufferSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);
        setGpuBufferName(modelBuffer, NAMEOF(modelBuffer));
    }

    if (!drawIndirectBuffer.buffer)
    {
        drawIndirectBuffer = createGpuBuffer(2 * aligned(sizeof(DrawIndirectData), sboAlignment),
            VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
        setGpuBufferName(drawIndirectBuffer, NAMEOF(drawIndirectBuffer));
    }

    if (!burnMapImage.image)
    {
        burnMapImage = createGpuImage(VK_FORMAT_R32G32B32A32_SFLOAT, { 2048, 2048 }, 1,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, GpuImageType::Image2D);
        setGpuImageName(burnMapImage, NAMEOF(burnMapImage));
    }

    model = loadSceneFromFile(sceneDirPath);

    for (uint8_t j = 0; j < countOf(materialInfos); j++)
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

    memcpy((char *)stagingBufferMappedData + indicesOffset, model.indices.data(), indicesSize);
    memcpy((char *)stagingBufferMappedData + positionsOffset, model.positions.data(), positionsSize);
    memcpy((char *)stagingBufferMappedData + normalUvsOffset, model.normalUvs.data(), normalUvsSize);
    memcpy((char *)stagingBufferMappedData + transformsOffset, model.transforms.data(), transformsSize);
    memcpy((char *)stagingBufferMappedData + materialsOffset, model.materials.data(), materialsSize);
    VkBufferCopy copies[]
    {
        {indicesOffset, maxReadIndicesOffset, indicesSize},
        {positionsOffset, maxPositionsOffset, positionsSize},
        {normalUvsOffset, maxNormalUvsOffset, normalUvsSize},
        {transformsOffset, maxTransformsOffset, transformsSize},
        {materialsOffset, maxMaterialsOffset, materialsSize}
    };
    copyStagingBufferToBuffer(modelBuffer, copies, countOf(copies));

    static_assert(offsetof(DrawIndirectData, vertexCount) == sizeof(VkDrawIndirectCommand), "");

    drawDataReadIndex = 0;
    drawIndirectReadData.indexCount = (uint32_t)model.indices.size();
    drawIndirectReadData.instanceCount = 1;
    drawIndirectReadData.firstVertex = 0;
    drawIndirectReadData.firstInstance = 0;
    drawIndirectReadData.vertexCount = (uint32_t)model.positions.size();
    memcpy(drawIndirectBuffer.mappedData, &drawIndirectReadData, sizeof(DrawIndirectData));

    VkDescriptorBufferInfo bufferInfos[]
    {
        {drawIndirectBuffer.buffer, 0, 2 * sizeof(DrawIndirectData)},
        {modelBuffer.buffer, maxReadIndicesOffset, maxIndicesSize},
        {modelBuffer.buffer, maxReadIndicesOffset, maxIndicesSize},
        {modelBuffer.buffer, maxPositionsOffset, maxPositionsSize},
        {modelBuffer.buffer, maxNormalUvsOffset, maxNormalUvsSize},
        {modelBuffer.buffer, maxTransformsOffset, maxTransformsSize},
        {modelBuffer.buffer, maxMaterialsOffset, maxMaterialsSize}
    };

    VkDescriptorImageInfo burnMapImageInfo { nullptr, burnMapImage.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

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
        modelTextures[i] = createAndCopyGpuImage(image, QueueFamily::Graphics, VK_IMAGE_USAGE_SAMPLED_BIT);
        destroyImage(image);
        imageInfos[i].imageView = modelTextures[i].imageView;
        imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    VkWriteDescriptorSet writes[]
    {
        initWriteDescriptorSetBuffer(globalDescriptorSet, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bufferInfos + 0),
        initWriteDescriptorSetBuffer(globalDescriptorSet, 1, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, bufferInfos + 1),
        initWriteDescriptorSetBuffer(globalDescriptorSet, 2, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, bufferInfos + 2),
        initWriteDescriptorSetBuffer(globalDescriptorSet, 3, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bufferInfos + 3),
        initWriteDescriptorSetBuffer(globalDescriptorSet, 4, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bufferInfos + 4),
        initWriteDescriptorSetBuffer(globalDescriptorSet, 5, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bufferInfos + 5),
        initWriteDescriptorSetBuffer(globalDescriptorSet, 6, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bufferInfos + 6),
        initWriteDescriptorSetImage(globalDescriptorSet, 24, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &burnMapImageInfo),
        initWriteDescriptorSetImage(globalDescriptorSet, 30, modelTextureCount, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, imageInfos)
    };

    vkUpdateDescriptorSets(device, countOf(writes), writes, 0, nullptr);

    beginOneTimeCmd(graphicsCmd);
    {
        ScopedGpuZoneAutoCollect(graphicsCmd, "Clear burn map image");
        VkClearColorValue clearValue {};
        VkImageSubresourceRange range = initImageSubresourceRange();
        ImageBarrier imageBarrier {};
        imageBarrier.image = burnMapImage;
        imageBarrier.srcStageMask = StageFlags::FragmentShader;
        imageBarrier.dstStageMask = StageFlags::Clear;
        imageBarrier.srcAccessMask = AccessFlags::Read;
        imageBarrier.dstAccessMask = AccessFlags::Write;
        imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        pipelineBarrier(graphicsCmd, nullptr, 0, &imageBarrier, 1);
        vkCmdClearColorImage(graphicsCmd.commandBuffer, burnMapImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1, &range);
        imageBarrier = {};
        imageBarrier.image = burnMapImage;
        imageBarrier.srcStageMask = StageFlags::Clear;
        imageBarrier.dstStageMask = StageFlags::FragmentShader;
        imageBarrier.srcAccessMask = AccessFlags::Write;
        imageBarrier.dstAccessMask = AccessFlags::Read;
        imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        pipelineBarrier(graphicsCmd, nullptr, 0, &imageBarrier, 1);
    }
    endAndSubmitOneTimeCmd(graphicsCmd, graphicsQueue);
}

void loadEnvMaps()
{
    ZoneScoped;

#if SKIP_ENV_MAP_REIMPORT
    if (!pathExists(brdfLutImagePath))
#endif
        computeBrdfLut(brdfLutImagePath);

    Image image = loadImage(brdfLutImagePath);
    brdfLut = createAndCopyGpuImage(image, QueueFamily::Graphics, VK_IMAGE_USAGE_SAMPLED_BIT);
    destroyImage(image);

    VkDescriptorImageInfo brdfLutImageInfo { nullptr, brdfLut.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    VkDescriptorImageInfo skyboxImageInfos[countOf(hdriImagePaths)];
    VkDescriptorImageInfo irradianceImageInfos[countOf(hdriImagePaths)];
    VkDescriptorImageInfo prefilteredImageInfos[countOf(hdriImagePaths)];
    char skyboxImagePath[256], irradianceMapImagePath[256], prefilteredMapImagePath[256];

    for (uint8_t i = 0; i < countOf(hdriImagePaths); i++)
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
        skyboxImages[i] = createAndCopyGpuImage(image, QueueFamily::Graphics, VK_IMAGE_USAGE_SAMPLED_BIT, GpuImageType::Image2DCubemap);
        destroyImage(image);
        image = loadImage(irradianceMapImagePath);
        irradianceMaps[i] = createAndCopyGpuImage(image, QueueFamily::Graphics, VK_IMAGE_USAGE_SAMPLED_BIT, GpuImageType::Image2DCubemap);
        destroyImage(image);
        image = loadImage(prefilteredMapImagePath);
        prefilteredMaps[i] = createAndCopyGpuImage(image, QueueFamily::Graphics, VK_IMAGE_USAGE_SAMPLED_BIT, GpuImageType::Image2DCubemap);
        destroyImage(image);

        skyboxImageInfos[i] = { nullptr, skyboxImages[i].imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        irradianceImageInfos[i] = { nullptr, irradianceMaps[i].imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        prefilteredImageInfos[i] = { nullptr, prefilteredMaps[i].imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    }

    VkWriteDescriptorSet writes[]
    {
        initWriteDescriptorSetImage(globalDescriptorSet, 20, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &brdfLutImageInfo),
        initWriteDescriptorSetImage(globalDescriptorSet, 21, countOf(skyboxImageInfos), VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, skyboxImageInfos),
        initWriteDescriptorSetImage(globalDescriptorSet, 22, countOf(irradianceImageInfos), VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, irradianceImageInfos),
        initWriteDescriptorSetImage(globalDescriptorSet, 23, countOf(prefilteredImageInfos), VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, prefilteredImageInfos),
    };

    vkUpdateDescriptorSets(device, countOf(writes), writes, 0, nullptr);
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
    uint32_t bufferSize = aligned(sizeof(LightingData), uboAlignment) + aligned(sizeof(SceneData), uboAlignment) * maxFramesInFlight;

    globalUniformBuffer = createGpuBuffer(bufferSize,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
    setGpuBufferName(globalUniformBuffer, NAMEOF(globalUniformBuffer));

    VkDescriptorBufferInfo bufferInfos[]
    {
        {globalUniformBuffer.buffer, 0, sizeof(LightingData)},
        {globalUniformBuffer.buffer, aligned(sizeof(LightingData), uboAlignment), aligned(sizeof(SceneData), uboAlignment)}
    };

    VkWriteDescriptorSet writes[]
    {
        initWriteDescriptorSetBuffer(globalDescriptorSet, 7, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, bufferInfos + 0),
        initWriteDescriptorSetBuffer(globalDescriptorSet, 8, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, bufferInfos + 1)
    };
    vkUpdateDescriptorSets(device, countOf(writes), writes, 0, nullptr);
}

void initScene()
{
    ZoneScoped;
    prepareAssets();

    loadModel(sceneInfos[selectedScene].sceneDirPath);
    loadEnvMaps();
    loadLights();

    lineData.width = defaultLineWidth * uiScale;
    cuttingData.width = 0.1f; // 10 cm
    camera.getPosition() = glm::vec3(0.f, 1.f, 5.f);
    camera.lookAt(0.5f * (model.aabb.min + model.aabb.max));
}

void terminateScene()
{
    destroyGpuBuffer(modelBuffer);
    destroyGpuBuffer(globalUniformBuffer);
    destroyGpuBuffer(drawIndirectBuffer);

    for (uint8_t i = 0; i < modelTextureCount; i++)
    {
        destroyGpuImage(modelTextures[i]);
    }

    for (uint8_t i = 0; i < countOf(hdriImagePaths); i++)
    {
        destroyGpuImage(skyboxImages[i]);
        destroyGpuImage(irradianceMaps[i]);
        destroyGpuImage(prefilteredMaps[i]);
    }

    destroyGpuImage(brdfLut);
    destroyGpuImage(burnMapImage);
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

    if (ImGui::GetIO().WantCaptureMouse || cursorCaptured)
        return;

    if (action == GLFW_PRESS)
    {
        if (button == GLFW_MOUSE_BUTTON_1 && cutState == CutState::None)
        {
            cursorCaptured = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }

        if (button == GLFW_MOUSE_BUTTON_2)
        {
            double x, y;

            switch (cutState)
            {
            case CutState::None:
                glfwGetCursorPos(window, &x, &y);
                lineData.p1 = glm::vec2(x, y);
                cutState = CutState::CutLineStarted;
                break;
            case CutState::CutLineStarted:
                glfwGetCursorPos(window, &x, &y);
                lineData.p2 = glm::vec2(x, y);
                cutState = CutState::CutLineFinished;
                break;
            default:
                break;
            }
        }
    }
}

bool compileShaders()
{
    ZoneScoped;
    ASSERT(pathExists(assetsPath) && "Assets must reside in the working dir!");

    for (uint8_t i = 0; i < sizeof(shaderTable) / sizeof(ShaderCompileInfo); i++)
    {
        const ShaderCompileInfo &info = ((ShaderCompileInfo *)&shaderTable)[i];
        if (!compileShaderIntoSpv(info.shaderSourcePath, info.shaderSpvPath, info.shaderType))
            return false;
    }

    return true;
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
    initShaderCompiler();
    VERIFY(compileShaders());
    initGraphics();
    initFrameData();
    initRenderTargets();
    initDescriptors();
    initPipelines();
    initImgui();
    initRenderdoc();
    initJobSystem();
    initImageUtils(shaderTable.computeSkyboxComputeShader.shaderSpvPath,
        shaderTable.computeBrdfLutComputeShader.shaderSpvPath,
        shaderTable.computeIrradianceMapComputeShader.shaderSpvPath,
        shaderTable.computePrefilteredMapComputeShader.shaderSpvPath,
        shaderTable.normalizeNormalMapComputeShader.shaderSpvPath);

    updateRenderTargetDescriptors();
    initScene();
}

void exit()
{
    vkDeviceWaitIdle(device);

    terminateScene();

    terminateImageUtils();
    terminateJobSystem();
    terminateImgui();
    terminatePipelines();
    terminateDescriptors();
    terminateRenderTargets();
    terminateFrameData();
    terminateGraphics();
    terminateShaderCompiler();
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

static glm::vec3 screenToWorld(glm::vec2 p, glm::vec2 res, glm::mat4 invViewProjMat)
{
    float clipX = p.x / res.x * 2.f - 1.f;
    float clipY = p.y / res.y * -2.f + 1.f;
    glm::vec4 pos = invViewProjMat * glm::vec4(clipX, clipY, 0.f, 1.f);

    return glm::vec3(pos) / pos.w;
}

void updateLogic(float delta)
{
    ZoneScoped;
    camera.update(delta);

    sceneRotationTime += delta * rotateScene;
    glm::mat4 sceneMat = glm::eulerAngleY(glm::half_pi<float>() * sceneRotationTime);
    float aspectRatio = (float)frameBufferImage.extent.width / frameBufferImage.extent.height;
    glm::mat4 projMat = glm::perspective(glm::radians(45.0f), aspectRatio, 1000.f, 0.1f);
    glm::mat4 invViewMat = camera.getCameraMatrix();
    glm::mat4 invProjMat = glm::inverse(projMat);

    FrameData &frame = frames[frameIndex];
    frame.sceneData.sceneMat = sceneMat;
    frame.sceneData.sceneConfig = sceneConfig;
    frame.sceneData.viewMat = camera.getViewMatrix();;
    frame.sceneData.projMat = projMat;
    frame.sceneData.invViewMat = invViewMat;
    frame.sceneData.invProjMat = invProjMat;

    switch (cutState)
    {
    case CutState::CutLineStarted:
    {
        double x, y;
        glfwGetCursorPos(window, &x, &y);
        lineData.p2 = glm::vec2(x, y);
        lineData.windowRes = glm::vec2(windowExtent.width, windowExtent.height);
        break;
    }
    case CutState::CutLineFinished:
    {
        glm::mat4 invViewProjMat = invViewMat * invProjMat;
        glm::vec3 p1 = screenToWorld(lineData.p1, lineData.windowRes, invViewProjMat);
        glm::vec3 p2 = screenToWorld(lineData.p2, lineData.windowRes, invViewProjMat);
        glm::vec3 p3 = camera.getPosition();

        glm::vec3 planeNormal = glm::normalize(glm::cross(p3 - p1, p3 - p2));
        float planeD = -glm::dot(planeNormal, p3);
        cuttingData.normalAndD = glm::vec4(planeNormal, planeD);
        cuttingData.drawDataReadIndex = drawDataReadIndex;
        break;
    }
    default:
        break;
    }

    if (sceneChangeRequired)
    {
        vkDeviceWaitIdle(device);
        loadModel(sceneInfos[selectedScene].sceneDirPath);
        camera.lookAt(0.5f * (model.aabb.min + model.aabb.max));
        sceneChangeRequired = false;
    }

    if (renderTargetsChangeRequired)
    {
        recreateRenderTargets();
        renderTargetsChangeRequired = false;
    }

    if (shaderReloadRequired)
    {
        lastShaderReloadSuccessful = compileShaders();

        if (lastShaderReloadSuccessful)
        {
            vkDeviceWaitIdle(device);
            terminatePipelines();
            initPipelines();
        }

        shaderReloadRequired = false;
    }

    if (imguiVulkanResetRequired)
    {
        terminateImguiVulkan(); // FIXME
        initImguiVulkan();
        imguiVulkanResetRequired = false;
    }
}

void update(float delta)
{
    updateInput();
    updateLogic(delta);
}

static struct DynamicOffsets
{
    uint32_t offsets[8];
    uint32_t offsetCount;
} dynamicOffsets;

void updateUniforms(const FrameData &frame)
{
    uint32_t uboAlignment = (uint32_t)physicalDeviceProperties.limits.minUniformBufferOffsetAlignment;
    uint32_t sboAlignment = (uint32_t)physicalDeviceProperties.limits.minStorageBufferOffsetAlignment;
    uint32_t cameraDataStaticOffset = aligned(sizeof(LightingData), uboAlignment);
    uint32_t cameraDataDynamicOffset = aligned(sizeof(SceneData), uboAlignment) * frameIndex;
    uint32_t maxIndicesOffset = aligned(maxIndicesSize, sboAlignment);
    uint32_t readIndicesOffset = drawDataReadIndex * maxIndicesOffset;
    uint32_t writeIndicesOffset = !drawDataReadIndex * maxIndicesOffset;

    dynamicOffsets.offsets[0] = readIndicesOffset;
    dynamicOffsets.offsets[1] = writeIndicesOffset;
    dynamicOffsets.offsets[2] = cameraDataDynamicOffset;
    dynamicOffsets.offsetCount = 3;

    memcpy(globalUniformBuffer.mappedData, &lightData, sizeof(LightingData));
    memcpy((char *)globalUniformBuffer.mappedData + cameraDataStaticOffset + cameraDataDynamicOffset, &frame.sceneData, sizeof(SceneData));
}

void drawModel(Cmd cmd)
{
    ZoneScoped;
    ScopedGpuZone(cmd, "Model");

    VkViewport viewport {};
    viewport.x = 0;
    viewport.y = (float)frameBufferImage.extent.height;
    viewport.width = (float)frameBufferImage.extent.width;
    viewport.height = -(float)frameBufferImage.extent.height;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;
    vkCmdSetViewport(cmd.commandBuffer, 0, 1, &viewport);

    VkRect2D scissor {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = frameBufferImage.extent.width;
    scissor.extent.height = frameBufferImage.extent.height;
    vkCmdSetScissor(cmd.commandBuffer, 0, 1, &scissor);

    vkCmdBindPipeline(cmd.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, modelPipeline);
    uint32_t drawIndirectDataReadOffset = drawDataReadIndex * sizeof(DrawIndirectData);
    vkCmdDrawIndirect(cmd.commandBuffer, drawIndirectBuffer.buffer, drawIndirectDataReadOffset, 1, 0);
}

void drawSkybox(Cmd cmd)
{
    ZoneScoped;
    ScopedGpuZone(cmd, "Skybox");
    vkCmdBindPipeline(cmd.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipeline);
    vkCmdDraw(cmd.commandBuffer, 3, 1, 0, 0);
}

void drawLine(Cmd cmd)
{
    ZoneScoped;
    ScopedGpuZone(cmd, "Line");

    VkViewport viewport {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = (float)frameBufferImage.extent.width;
    viewport.height = (float)frameBufferImage.extent.height;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;
    vkCmdSetViewport(cmd.commandBuffer, 0, 1, &viewport);

    VkRect2D scissor {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = frameBufferImage.extent.width;
    scissor.extent.height = frameBufferImage.extent.height;
    vkCmdSetScissor(cmd.commandBuffer, 0, 1, &scissor);

    vkCmdBindPipeline(cmd.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, linePipeline);
    vkCmdDraw(cmd.commandBuffer, 6, 1, 0, 0);
}

static inline void tableCellLabel(const char *label)
{
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::TableNextColumn();
}

void drawImgui(Cmd cmd)
{
    ZoneScoped;
    ScopedGpuZone(cmd, "ImGui");
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

#pragma region UiCode
    //ImGui::ShowDemoWindow();
    ImGui::Begin(appName, nullptr, ImGuiWindowFlags_AlwaysAutoResize);

    if (ImGui::BeginTable("Table", 2, ImGuiTableFlags_NoHostExtendX))
    {
        const char *basename;
        cwk_path_get_basename(sceneInfos[selectedScene].sceneDirPath, &basename, nullptr);
        tableCellLabel("Model");
        if (ImGui::BeginCombo("##Model", basename, ImGuiComboFlags_WidthFitPreview))
        {
            for (uint8_t i = 0; i < countOf(sceneInfos); i++)
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

        tableCellLabel("Material");
        if (ImGui::BeginCombo("##Material", materialNames[selectedMaterial], ImGuiComboFlags_WidthFitPreview))
        {
            for (uint8_t i = 0; i < countOf(materialNames); i++)
            {
                bool selected = i == selectedMaterial;
                if (ImGui::Selectable(materialNames[i], selected) && !selected)
                {
                    selectedMaterial = i;
                }
            }
            ImGui::EndCombo();
        }

        cwk_path_get_basename(hdriImagePaths[selectedSkybox], &basename, nullptr);
        tableCellLabel("Skybox");
        if (ImGui::BeginCombo("##Skybox", basename, ImGuiComboFlags_WidthFitPreview))
        {
            for (uint8_t i = 0; i < countOf(hdriImagePaths); i++)
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
        ImGui::TableNextColumn();
        sceneChangeRequired = sceneChangeRequired || ImGui::Button("Reset model");

        ImGui::TableNextColumn();
        ImGui::TableNextColumn();
        shaderReloadRequired = ImGui::Button("Reload shaders");
        ImGui::SameLine(0, 0);
        ImGui::PushStyleColor(ImGuiCol_Text, lastShaderReloadSuccessful ? IM_COL32(0, 255, 0, 255) : IM_COL32(255, 0, 0, 255));
        ImGui::Bullet();
        ImGui::PopStyleColor();

        tableCellLabel("Cut width");
        ImGui::SliderFloat("", &cuttingData.width, 0.1f, 0.5f, "%.2f", ImGuiSliderFlags_AlwaysClamp);

        tableCellLabel("Rotate model");
        ImGui::Checkbox("##Rotate model", &rotateScene);
        tableCellLabel("Show wireframe");
        ImGui::CheckboxFlags("##Show wireframe", &sceneConfig, SCENE_SHOW_WIREFRAME);
        tableCellLabel("Use normal map");
        ImGui::CheckboxFlags("##Use normal map", &sceneConfig, SCENE_USE_NORMAL_MAP);
        tableCellLabel("Use lights");
        ImGui::CheckboxFlags("##Use lights", &sceneConfig, SCENE_USE_LIGHTS);
        tableCellLabel("Use IBL");
        ImGui::CheckboxFlags("##Use IBL", &sceneConfig, SCENE_USE_IBL);

        tableCellLabel("Bloom strength");
        ImGui::SliderFloat("##Bloom", &bloomStrength, 0.f, 1.f, "%.2f", ImGuiSliderFlags_AlwaysClamp);

        tableCellLabel("VSync");
        if (ImGui::Checkbox("##VSync", &vSyncOn))
            renderTargetsChangeRequired = true;

        static const uint32_t msaaLevels[] { 2, 4, 8, 16 };
        static const char *const msaaLevelNames[] { "2", "4", "8", "16" };
        static uint8_t selectedMsaaLevel = 0;

        tableCellLabel("MSAA");
        if (ImGui::BeginCombo("##MSAA", msaaLevelNames[selectedMsaaLevel], ImGuiComboFlags_WidthFitPreview))
        {
            for (uint8_t i = 0; i < countOf(msaaLevelNames) && msaaLevels[i] <= maxMsaaSampleCount; i++)
            {
                bool selected = i == selectedMsaaLevel;
                if (ImGui::Selectable(msaaLevelNames[i], selected) && !selected)
                {
                    selectedMsaaLevel = i;
                    msaaSampleCount = msaaLevels[selectedMsaaLevel];
                    shaderReloadRequired = true;
                    renderTargetsChangeRequired = true;
                    imguiVulkanResetRequired = true;
                }
            }
            ImGui::EndCombo();
        }

        char buffer[64];
        snprintf(buffer, sizeof(buffer), "%u/%u", drawIndirectReadData.vertexCount, maxVertexCount);
        tableCellLabel("Vertex buffer");
        ImGui::ProgressBar((float)drawIndirectReadData.vertexCount / maxVertexCount, ImVec2(-FLT_MIN, 0), buffer);

        snprintf(buffer, sizeof(buffer), "%u/%u", drawIndirectReadData.indexCount, maxIndexCount);
        tableCellLabel("Index buffer");
        ImGui::ProgressBar((float)drawIndirectReadData.indexCount / maxIndexCount, ImVec2(-FLT_MIN, 0), buffer);

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
            DEBUG_SHOW_SPECULAR,
            DEBUG_SHOW_BURN
        };
        static const char *const debugChannelNames[] { "None", "Color", "Normal", "AO", "Roughness", "Metallic", "Irradiance", "Prefiltered", "Diffuse", "Specular", "Burn" };
        static uint8_t selectedChannel = 0;
        static_assert(countOf(debugChannels) == countOf(debugChannelNames), "");

        tableCellLabel("Debug Channel");
        if (ImGui::BeginCombo("##Debug Channel", debugChannelNames[selectedChannel], ImGuiComboFlags_WidthFitPreview))
        {
            for (uint8_t i = 0; i < countOf(debugChannelNames); i++)
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
#pragma endregion UiCode

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd.commandBuffer);
}

void dispatchCutting()
{
    ZoneScoped;
    vkVerify(vkResetFences(device, 1, &computeFence));
    vkVerify(vkResetCommandBuffer(computeCmd.commandBuffer, 0));

    DrawIndirectData drawIndirectWriteData = drawIndirectReadData;
    drawIndirectWriteData.indexCount = 0;
    uint32_t drawIndirectDataWriteOffset = !drawDataReadIndex * sizeof(DrawIndirectData);
    memcpy((char *)drawIndirectBuffer.mappedData + drawIndirectDataWriteOffset, &drawIndirectWriteData, sizeof(DrawIndirectData));

    beginOneTimeCmd(computeCmd);
    {
        ScopedGpuZoneAutoCollect(computeCmd, "Cutting");
        ASSERT(drawIndirectReadData.indexCount % 3 == 0);
        uint32_t groupSizeX = 256;
        uint32_t groupCountX = (drawIndirectReadData.indexCount / 3 + groupSizeX - 1) / groupSizeX;
        vkCmdBindDescriptorSets(computeCmd.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 1, &globalDescriptorSet, dynamicOffsets.offsetCount, dynamicOffsets.offsets);
        vkCmdPushConstants(computeCmd.commandBuffer, computePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CuttingData), &cuttingData);
        vkCmdBindPipeline(computeCmd.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, cuttingPipeline);
        vkCmdDispatch(computeCmd.commandBuffer, groupCountX, 1, 1);
    }
    endAndSubmitOneTimeCmd(computeCmd, computeQueue, nullptr, nullptr, computeFence);
}

void readCuttingData()
{
    drawDataReadIndex = !drawDataReadIndex; // swap draw and index buffers
    uint32_t drawIndirectDataReadOffset = drawDataReadIndex * sizeof(DrawIndirectData);
    memcpy(&drawIndirectReadData, (char *)drawIndirectBuffer.mappedData + drawIndirectDataReadOffset, sizeof(DrawIndirectData));
}

void burnMapPass(Cmd cmd)
{
    ZoneScoped;
    ScopedGpuZone(cmd, __FUNCTION__);

    VkExtent2D extent { burnMapImage.extent.width, burnMapImage.extent.height };
    VkRenderingAttachmentInfoKHR colorAttachmentInfo = initRenderingAttachmentInfo(burnMapImage.imageView, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, nullptr);
    VkRenderingInfoKHR renderingInfo = initRenderingInfo(extent, &colorAttachmentInfo, 1, nullptr);

    ImageBarrier imageBarrier {};
    imageBarrier.image = burnMapImage;
    imageBarrier.srcStageMask = StageFlags::FragmentShader;
    imageBarrier.dstStageMask = StageFlags::ColorAttachmentOutput;
    imageBarrier.srcAccessMask = AccessFlags::Read;
    imageBarrier.dstAccessMask = AccessFlags::Read | AccessFlags::Write;
    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    pipelineBarrier(cmd, nullptr, 0, &imageBarrier, 1);

    vkCmdBeginRenderingKHR(cmd.commandBuffer, &renderingInfo);

    PushData pushData { selectedSkybox, selectedMaterial, timeSinceStart, debugFlags, lineData, cuttingData };
    vkCmdPushConstants(cmd.commandBuffer, graphicsPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushData), &pushData);
    vkCmdBindDescriptorSets(cmd.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineLayout, 0, 1, &globalDescriptorSet, dynamicOffsets.offsetCount, dynamicOffsets.offsets);

    VkViewport viewport {};
    viewport.x = 0;
    viewport.y = (float)burnMapImage.extent.height;
    viewport.width = (float)burnMapImage.extent.width;
    viewport.height = -(float)burnMapImage.extent.height;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;
    vkCmdSetViewport(cmd.commandBuffer, 0, 1, &viewport);

    VkRect2D scissor {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = extent;
    vkCmdSetScissor(cmd.commandBuffer, 0, 1, &scissor);

    vkCmdBindPipeline(cmd.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, burnMapPipeline);
    uint32_t drawIndirectDataReadOffset = drawDataReadIndex * sizeof(DrawIndirectData);
    vkCmdDrawIndirect(cmd.commandBuffer, drawIndirectBuffer.buffer, drawIndirectDataReadOffset, 1, 0);

    vkCmdEndRenderingKHR(cmd.commandBuffer);

    imageBarrier = {};
    imageBarrier.image = burnMapImage;
    imageBarrier.srcStageMask = StageFlags::ColorAttachmentOutput;
    imageBarrier.dstStageMask = StageFlags::FragmentShader;
    imageBarrier.srcAccessMask = AccessFlags::Write;
    imageBarrier.dstAccessMask = AccessFlags::Read;
    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    pipelineBarrier(cmd, nullptr, 0, &imageBarrier, 1);
}

void mainGeometryPass(Cmd cmd)
{
    ZoneScoped;
    ScopedGpuZone(cmd, __FUNCTION__);
    VkClearValue colorClearValue {}, depthClearValue {};
    VkExtent2D extent { frameBufferImage.extent.width, frameBufferImage.extent.height };
    VkRenderingAttachmentInfoKHR colorAttachmentInfo = initRenderingAttachmentInfo(frameBufferImage.imageView, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, &colorClearValue, VK_ATTACHMENT_STORE_OP_STORE, bloomImageMipImageViews[0]);
    VkRenderingAttachmentInfoKHR depthAttachmentInfo = initRenderingAttachmentInfo(depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, &depthClearValue, VK_ATTACHMENT_STORE_OP_DONT_CARE);
    VkRenderingInfoKHR renderingInfo = initRenderingInfo(extent, &colorAttachmentInfo, 1, &depthAttachmentInfo);

    ImageBarrier imageBarriers[2] {};
    imageBarriers[0].image = frameBufferImage;
    imageBarriers[0].srcStageMask = StageFlags::ColorAttachmentOutput;
    imageBarriers[0].dstStageMask = StageFlags::ColorAttachmentOutput;
    imageBarriers[0].srcAccessMask = AccessFlags::Read | AccessFlags::Write;
    imageBarriers[0].dstAccessMask = AccessFlags::Read | AccessFlags::Write;
    imageBarriers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageBarriers[0].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    imageBarriers[0].baseMipLevel = 0;
    imageBarriers[0].levelCount = 1;
    imageBarriers[1].image = bloomImage;
    imageBarriers[1].srcStageMask = StageFlags::ComputeShader;
    imageBarriers[1].dstStageMask = StageFlags::ColorAttachmentOutput;
    imageBarriers[1].srcAccessMask = AccessFlags::Read;
    imageBarriers[1].dstAccessMask = AccessFlags::Write;
    imageBarriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageBarriers[1].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    imageBarriers[1].baseMipLevel = 0;
    imageBarriers[1].levelCount = 1;
    pipelineBarrier(cmd, nullptr, 0, imageBarriers, 2);

    vkCmdBeginRenderingKHR(cmd.commandBuffer, &renderingInfo);
    PushData pushData { selectedSkybox, selectedMaterial, timeSinceStart, debugFlags, lineData, cuttingData };
    vkCmdPushConstants(cmd.commandBuffer, graphicsPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushData), &pushData);
    vkCmdBindDescriptorSets(cmd.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineLayout, 0, 1, &globalDescriptorSet, dynamicOffsets.offsetCount, dynamicOffsets.offsets);

    drawModel(cmd);
    drawSkybox(cmd);
    vkCmdEndRenderingKHR(cmd.commandBuffer);
}

void progressiveBlurPass(Cmd cmd)
{
    ZoneScoped;
    ScopedGpuZone(cmd, __FUNCTION__);

    blitGpuImageMips(cmd, bloomImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, StageFlags::ColorAttachmentOutput, StageFlags::ComputeShader);

    vkCmdBindDescriptorSets(cmd.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 1, &globalDescriptorSet, dynamicOffsets.offsetCount, dynamicOffsets.offsets);
    vkCmdBindPipeline(cmd.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, blurPipeline);

    for (uint32_t i = bloomImage.levelCount - 1; i > 0; i--)
    {
        ImageBarrier imageBarrier {};
        imageBarrier.image = bloomImage;
        imageBarrier.srcStageMask = StageFlags::ComputeShader;
        imageBarrier.dstStageMask = StageFlags::ComputeShader;
        imageBarrier.srcAccessMask = AccessFlags::Read | AccessFlags::Write;
        imageBarrier.dstAccessMask = AccessFlags::Read;
        imageBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // not performing this transition causes very weird behaviour on my GPU. Might be a driver bug.
        imageBarrier.levelCount = 1;
        imageBarrier.baseMipLevel = i;
        pipelineBarrier(cmd, nullptr, 0, &imageBarrier, 1);

        uint32_t countX = (bloomImageMipSizes[i - 1].x + 15) / 16;
        uint32_t countY = (bloomImageMipSizes[i - 1].y + 15) / 16;
        vkCmdPushConstants(cmd.commandBuffer, computePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &i);
        vkCmdDispatch(cmd.commandBuffer, countX, countY, 1);
    }
}

void bloomAndTonemapPass(Cmd cmd)
{
    ZoneScoped;
    ScopedGpuZone(cmd, __FUNCTION__);

    ImageBarrier imageBarriers[2] {};
    imageBarriers[0].image = frameBufferImage;
    imageBarriers[0].srcStageMask = StageFlags::ColorAttachmentOutput;
    imageBarriers[0].dstStageMask = StageFlags::ComputeShader;
    imageBarriers[0].srcAccessMask = AccessFlags::Read | AccessFlags::Write;
    imageBarriers[0].dstAccessMask = AccessFlags::Read | AccessFlags::Write;
    imageBarriers[0].oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    imageBarriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageBarriers[0].baseMipLevel = 0;
    imageBarriers[0].levelCount = 1;
    imageBarriers[1].image = bloomImage;
    imageBarriers[1].srcStageMask = StageFlags::ComputeShader;
    imageBarriers[1].dstStageMask = StageFlags::ComputeShader;
    imageBarriers[1].srcAccessMask = AccessFlags::Read | AccessFlags::Write;
    imageBarriers[1].dstAccessMask = AccessFlags::Read;
    imageBarriers[1].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageBarriers[1].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageBarriers[1].baseMipLevel = 0;
    imageBarriers[1].levelCount = 1;
    pipelineBarrier(cmd, nullptr, 0, imageBarriers, 2);

    vkCmdBindPipeline(cmd.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, bloomAndTonemapPipeline);
    vkCmdPushConstants(cmd.commandBuffer, computePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(float), &bloomStrength);
    uint32_t countX = (bloomImageMipSizes[0].x + 15) / 16;
    uint32_t countY = (bloomImageMipSizes[0].y + 15) / 16;
    vkCmdDispatch(cmd.commandBuffer, countX, countY, 1);
}

void uiPass(Cmd cmd)
{
    ZoneScoped;
    ScopedGpuZone(cmd, __FUNCTION__);
    ImageBarrier imageBarriers[2] {};
    imageBarriers[0].image = frameBufferImage;
    imageBarriers[0].srcStageMask = StageFlags::ComputeShader;
    imageBarriers[0].dstStageMask = StageFlags::ColorAttachmentOutput;
    imageBarriers[0].srcAccessMask = AccessFlags::Read | AccessFlags::Write;
    imageBarriers[0].dstAccessMask = AccessFlags::Read | AccessFlags::Write;
    imageBarriers[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageBarriers[0].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    imageBarriers[0].baseMipLevel = 0;
    imageBarriers[0].levelCount = 1;
    imageBarriers[1].image = resolveImage;
    imageBarriers[1].srcStageMask = StageFlags::Blit;
    imageBarriers[1].dstStageMask = StageFlags::ColorAttachmentOutput;
    imageBarriers[1].srcAccessMask = AccessFlags::Read;
    imageBarriers[1].dstAccessMask = AccessFlags::Write;
    imageBarriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageBarriers[1].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    imageBarriers[1].baseMipLevel = 0;
    imageBarriers[1].levelCount = 1;
    pipelineBarrier(cmd, nullptr, 0, imageBarriers, 2);

    VkExtent2D extent { frameBufferImage.extent.width, frameBufferImage.extent.height };
    VkRenderingAttachmentInfoKHR colorAttachmentInfo = initRenderingAttachmentInfo(frameBufferImage.imageView, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, nullptr, VK_ATTACHMENT_STORE_OP_STORE, resolveImage.imageView);
    VkRenderingInfoKHR renderingInfo = initRenderingInfo(extent, &colorAttachmentInfo, 1, nullptr);

    vkCmdBeginRenderingKHR(cmd.commandBuffer, &renderingInfo);

    if (cutState == CutState::CutLineStarted)
    {
        drawLine(cmd);
    }

    drawImgui(cmd);

    vkCmdEndRenderingKHR(cmd.commandBuffer);
}

void blitResolveToSwapchain(Cmd cmd, uint32_t swapchainImageIndex)
{
    ZoneScoped;
    ScopedGpuZone(cmd, __FUNCTION__);
    ImageBarrier imageBarriers[2] {};
    imageBarriers[0].image = resolveImage;
    imageBarriers[0].srcStageMask = StageFlags::ColorAttachmentOutput;
    imageBarriers[0].dstStageMask = StageFlags::Blit;
    imageBarriers[0].srcAccessMask = AccessFlags::Read | AccessFlags::Write;
    imageBarriers[0].dstAccessMask = AccessFlags::Read;
    imageBarriers[0].oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    imageBarriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    imageBarriers[0].baseMipLevel = 0;
    imageBarriers[0].levelCount = 1;

    imageBarriers[1].image.image = swapchainImages[swapchainImageIndex];
    imageBarriers[1].srcStageMask = StageFlags::Clear; // dummy stage to wait for the imageAcquiredSemaphore
    imageBarriers[1].dstStageMask = StageFlags::Blit;
    imageBarriers[1].srcAccessMask = AccessFlags::None;
    imageBarriers[1].dstAccessMask = AccessFlags::Write;
    imageBarriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageBarriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    pipelineBarrier(cmd, nullptr, 0, imageBarriers, 2);

    VkImageBlit blitRegion {};
    blitRegion.srcSubresource = initImageSubresourceLayers();
    blitRegion.dstSubresource = initImageSubresourceLayers();
    blitRegion.srcOffsets[1] = *(VkOffset3D *)&resolveImage.extent;
    blitRegion.dstOffsets[1] = *(VkOffset3D *)&resolveImage.extent;
    vkCmdBlitImage(cmd.commandBuffer, resolveImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blitRegion, VK_FILTER_NEAREST);

    imageBarriers[0] = {};
    imageBarriers[0].image.image = swapchainImages[swapchainImageIndex];
    imageBarriers[0].srcStageMask = StageFlags::Blit;
    imageBarriers[0].dstStageMask = StageFlags::Clear; // dummy stage to make renderFinishedSemaphore wait
    imageBarriers[0].srcAccessMask = AccessFlags::Write;
    imageBarriers[0].dstAccessMask = AccessFlags::None;
    imageBarriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageBarriers[0].newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    pipelineBarrier(cmd, nullptr, 0, imageBarriers, 1);
}

static bool burnMapPassRequired = false;

void draw()
{
    ZoneScoped;

    if (cutState == CutState::CuttingInProgress)
    {
        if (vkGetFenceStatus(device, computeFence) == VK_SUCCESS)
        {
            readCuttingData();
            cutState = CutState::None;
        }
    }

    FrameData &frame = frames[frameIndex];
    updateUniforms(frame);

    if (cutState == CutState::CutLineFinished)
    {
        dispatchCutting();
        cutState = CutState::CuttingInProgress;
        burnMapPassRequired = true;
    }

    vkVerify(vkWaitForFences(device, 1, &frame.renderFinishedFence, true, UINT64_MAX));
    uint32_t swapchainImageIndex;
    VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, frame.imageAcquiredSemaphore, nullptr, &swapchainImageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        // dummy submit to wait for the semaphore
        VkSemaphoreSubmitInfoKHR waitSemaphoreSubmitInfo = initSemaphoreSubmitInfo(frame.imageAcquiredSemaphore, VK_PIPELINE_STAGE_2_NONE_KHR);
        VkSubmitInfo2 submitInfo = initSubmitInfo(nullptr, &waitSemaphoreSubmitInfo, nullptr);
        vkVerify(vkQueueSubmit2KHR(graphicsQueue, 1, &submitInfo, nullptr));
        renderTargetsChangeRequired = true;
        return;
    }

    vkAssert(result);
    vkVerify(vkResetFences(device, 1, &frame.renderFinishedFence));
    vkVerify(vkResetCommandBuffer(frame.cmd.commandBuffer, 0));

    beginOneTimeCmd(frame.cmd);
    {
        ScopedGpuZoneAutoCollect(frame.cmd, "Draw");

        if (burnMapPassRequired)
        {
            burnMapPass(frame.cmd);
            burnMapPassRequired = false;
        }

        mainGeometryPass(frame.cmd);
        progressiveBlurPass(frame.cmd);
        bloomAndTonemapPass(frame.cmd);
        uiPass(frame.cmd);
        blitResolveToSwapchain(frame.cmd, swapchainImageIndex);
    }
    VkSemaphoreSubmitInfoKHR waitSemaphoreSubmitInfo = initSemaphoreSubmitInfo(frame.imageAcquiredSemaphore, VK_PIPELINE_STAGE_2_CLEAR_BIT_KHR); // dummy stage
    VkSemaphoreSubmitInfoKHR signalSemaphoreSubmitInfo = initSemaphoreSubmitInfo(frame.renderFinishedSemaphore, VK_PIPELINE_STAGE_2_CLEAR_BIT_KHR); // dummy stage
    endAndSubmitOneTimeCmd(frame.cmd, graphicsQueue, &waitSemaphoreSubmitInfo, &signalSemaphoreSubmitInfo, frame.renderFinishedFence);

    VkPresentInfoKHR presentInfo = initPresentInfo(&swapchain, &frame.renderFinishedSemaphore, &swapchainImageIndex);
    result = vkQueuePresentKHR(graphicsQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        renderTargetsChangeRequired = true;
    }
    else
    {
        vkAssert(result);
    }

    frameIndex = (frameIndex + 1) % maxFramesInFlight;
}