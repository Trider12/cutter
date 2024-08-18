#include "Graphics.hpp"
#include "DebugUtils.hpp"
#include "ImageUtils.hpp"
#include "JobSystem.hpp"
#include "ShaderUtils.hpp"
#include "Utils.hpp"
#include "VkUtils.hpp"
#include "../src/shaders/common.h"

#include <cmp_core.h>
#define KHRONOS_STATIC
#include <ktx.h>
#define STBI_NO_STDIO
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_HDR
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define MIPS_BLIT

extern VkPhysicalDeviceProperties physicalDeviceProperties;
extern VkDevice device;
extern VkDescriptorPool descriptorPool;
extern VmaAllocator allocator;

extern VkQueue graphicsQueue;
extern VkQueue computeQueue;

static VkPipelineLayout commonPipelineLayout;
static VkPipeline computeSkyboxPipeline;
static VkPipeline computeBrdfLutPipeline;
static VkPipeline computeIrradianceMapPipeline;
static VkPipeline computePrefilteredMapPipeline;
static VkPipeline normalizeNormalMapPipeline;

static VkDescriptorSetLayout commonDescriptorSetLayout;
static VkDescriptorSet commonDescriptorSet;
static VkSampler linearClampSampler;

static constexpr uint16_t skyboxFaceSize = 2048;
static constexpr uint16_t brdfLutSize = 512;
static constexpr uint16_t irradianceMapFaceSize = 32;
static constexpr uint16_t prefilteredMapFaceSize = 128;
static constexpr uint32_t prefilteredMapLevelCount = MAX_PREFILTERED_MAP_LOD + 1;

static uint32_t maxWorkGroupSize2d = 0;

static constexpr float defaultCompressionQuality = 0.1f;
static void *optionsBC5;
static void *optionsBC6;
static void *optionsBC7;

static inline void findMaxWorkGroupSize2d()
{
    maxWorkGroupSize2d = min(physicalDeviceProperties.limits.maxComputeWorkGroupSize[0], physicalDeviceProperties.limits.maxComputeWorkGroupSize[1]);
    uint32_t maxInvocations = physicalDeviceProperties.limits.maxComputeWorkGroupInvocations;

    while (maxInvocations < maxWorkGroupSize2d * maxWorkGroupSize2d)
    {
        maxWorkGroupSize2d /= 2;
    }
}

static inline uint32_t findBestWorkGroupSize2d(uint32_t workSize)
{
    ASSERT(maxWorkGroupSize2d);
    return min(workSize, maxWorkGroupSize2d);
}

static inline uint32_t findBestWorkGroupCount2d(uint32_t workSize)
{
    ASSERT(maxWorkGroupSize2d);
    uint32_t workGroupSize = findBestWorkGroupSize2d(workSize);
    return (workSize + workGroupSize - 1) / workGroupSize;
}

static inline uint8_t calcMipLevelCount(uint16_t width, uint16_t height)
{
    return (uint8_t)(log2f((float)max(width, height))) + 1;
}

void initImageUtils(const char *computeSkyboxShaderPath, const char *computeBrdfLutShaderPath, const char *computeIrradianceMapShaderPath, const char *computePrefilteredMapShaderPath, const char *normalizeNormalMapShaderPath)
{
    ASSERT(isValidString(computeSkyboxShaderPath));
    ASSERT(isValidString(computeBrdfLutShaderPath));
    ASSERT(isValidString(computeIrradianceMapShaderPath));
    ASSERT(isValidString(computePrefilteredMapShaderPath));
    ASSERT(isValidString(normalizeNormalMapShaderPath));
    ASSERT(calcMipLevelCount(prefilteredMapFaceSize, prefilteredMapFaceSize) >= prefilteredMapLevelCount);

    findMaxWorkGroupSize2d();

    VkSamplerCreateInfo samplerCreateInfo = initSamplerCreateInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);
    vkVerify(vkCreateSampler(device, &samplerCreateInfo, nullptr, &linearClampSampler));

    // one set to rule them all
    VkDescriptorSetLayoutBinding setBindings[]
    {
        {0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT}, // hdri equirect
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT}, // skybox write | brdf lut | normal map
        {2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT}, // skybox read
        {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT}, // irradiance
        {4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, prefilteredMapLevelCount, VK_SHADER_STAGE_COMPUTE_BIT}, // prefiltered normalMapImage array
        {5, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, &linearClampSampler}
    };
    VkDescriptorBindingFlags bindingFlags[countOf(setBindings)] {};
    bindingFlags[3] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
    VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo = initDescriptorSetLayoutBindingFlagsCreateInfo(bindingFlags, countOf(bindingFlags));
    VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo = initDescriptorSetLayoutCreateInfo(setBindings, countOf(setBindings), &flagsInfo);
    vkVerify(vkCreateDescriptorSetLayout(device, &setLayoutCreateInfo, nullptr, &commonDescriptorSetLayout));
    VkDescriptorSetAllocateInfo setAllocateInfo = initDescriptorSetAllocateInfo(descriptorPool, &commonDescriptorSetLayout, 1);
    vkVerify(vkAllocateDescriptorSets(device, &setAllocateInfo, &commonDescriptorSet));

    VkPushConstantRange range {};
    range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    range.size = sizeof(uint32_t);
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = initPipelineLayoutCreateInfo(&commonDescriptorSetLayout, 1, &range, 1);
    vkVerify(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &commonPipelineLayout));

    uint32_t computeSkyboxWorkGroupSize = findBestWorkGroupSize2d(skyboxFaceSize);
    uint32_t computeBrdfLutWorkGroupSize = findBestWorkGroupSize2d(brdfLutSize);
    uint32_t computeIrradianceMapWorkGroupSize = findBestWorkGroupSize2d(irradianceMapFaceSize);
    uint32_t computePrefilteredMapWorkGroupSize = findBestWorkGroupSize2d(prefilteredMapFaceSize);

    VkSpecializationMapEntry specEntry { 0, 0, sizeof(uint32_t) };
    VkSpecializationInfo specInfos[]
    {
        { 1, &specEntry, sizeof(uint32_t), &computeSkyboxWorkGroupSize },
        { 1, &specEntry, sizeof(uint32_t), &computeBrdfLutWorkGroupSize },
        { 1, &specEntry, sizeof(uint32_t), &computeIrradianceMapWorkGroupSize },
        { 1, &specEntry, sizeof(uint32_t), &computePrefilteredMapWorkGroupSize },
        { 1, &specEntry, sizeof(uint32_t), &maxWorkGroupSize2d },
    };
    VkShaderModule shaderModules[]
    {
        createShaderModuleFromSpv(device, computeSkyboxShaderPath),
        createShaderModuleFromSpv(device, computeBrdfLutShaderPath),
        createShaderModuleFromSpv(device, computeIrradianceMapShaderPath),
        createShaderModuleFromSpv(device, computePrefilteredMapShaderPath),
        createShaderModuleFromSpv(device, normalizeNormalMapShaderPath)
    };
    VkPipelineShaderStageCreateInfo shaderStageCreateInfos[]
    {
        initPipelineShaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT, shaderModules[0], &specInfos[0]),
        initPipelineShaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT, shaderModules[1], &specInfos[1]),
        initPipelineShaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT, shaderModules[2], &specInfos[2]),
        initPipelineShaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT, shaderModules[3], &specInfos[3]),
        initPipelineShaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT, shaderModules[4], &specInfos[4])
    };
    VkComputePipelineCreateInfo computePipelineCreateInfos[]
    {
        initComputePipelineCreateInfo(shaderStageCreateInfos[0], commonPipelineLayout),
        initComputePipelineCreateInfo(shaderStageCreateInfos[1], commonPipelineLayout),
        initComputePipelineCreateInfo(shaderStageCreateInfos[2], commonPipelineLayout),
        initComputePipelineCreateInfo(shaderStageCreateInfos[3], commonPipelineLayout),
        initComputePipelineCreateInfo(shaderStageCreateInfos[4], commonPipelineLayout)
    };
    VkPipeline pipelines[countOf(computePipelineCreateInfos)];
    vkVerify(vkCreateComputePipelines(device, nullptr, countOf(computePipelineCreateInfos), computePipelineCreateInfos, nullptr, pipelines));
    computeSkyboxPipeline = pipelines[0];
    computeBrdfLutPipeline = pipelines[1];
    computeIrradianceMapPipeline = pipelines[2];
    computePrefilteredMapPipeline = pipelines[3];
    normalizeNormalMapPipeline = pipelines[4];

    for (uint8_t i = 0; i < countOf(shaderModules); i++)
    {
        vkDestroyShaderModule(device, shaderModules[i], nullptr);
    }

    CreateOptionsBC5(&optionsBC5);
    SetQualityBC5(optionsBC5, defaultCompressionQuality);
    CreateOptionsBC6(&optionsBC6);
    SetQualityBC6(optionsBC6, defaultCompressionQuality);
    CreateOptionsBC7(&optionsBC7);
    SetQualityBC7(optionsBC7, defaultCompressionQuality);
}

void terminateImageUtils()
{
    vkDestroySampler(device, linearClampSampler, nullptr);
    vkDestroyDescriptorSetLayout(device, commonDescriptorSetLayout, nullptr);
    vkDestroyPipelineLayout(device, commonPipelineLayout, nullptr);
    vkDestroyPipeline(device, computeSkyboxPipeline, nullptr);
    vkDestroyPipeline(device, computeBrdfLutPipeline, nullptr);
    vkDestroyPipeline(device, computeIrradianceMapPipeline, nullptr);
    vkDestroyPipeline(device, computePrefilteredMapPipeline, nullptr);
    vkDestroyPipeline(device, normalizeNormalMapPipeline, nullptr);

    DestroyOptionsBC5(optionsBC5);
    DestroyOptionsBC5(optionsBC6);
    DestroyOptionsBC5(optionsBC7);
}

static void normalizeNormalMap(Image &image);

static Image importImage(const uint8_t *data, uint32_t dataSize, ImagePurpose purpose)
{
    ZoneScoped;
    ASSERT(data);
    ASSERT(dataSize);
    ASSERT(purpose != ImagePurpose::Undefined);
    Image image {};
    int w = 0, h = 0;

    switch (purpose)
    {
    case ImagePurpose::Color:
        image.data = stbi_load_from_memory(data, dataSize, &w, &h, nullptr, STBI_rgb_alpha);
        image.dataSize = w * h * STBI_rgb_alpha;
        image.format = VK_FORMAT_R8G8B8A8_SRGB;
        break;
    case ImagePurpose::Normal:
    case ImagePurpose::Shading:
        image.data = stbi_load_from_memory(data, dataSize, &w, &h, nullptr, STBI_rgb_alpha);
        image.dataSize = w * h * STBI_rgb_alpha;
        image.format = VK_FORMAT_R8G8B8A8_UNORM;
        break;
    case ImagePurpose::HDRI:
        ASSERT(stbi_is_hdr_from_memory((const uint8_t *)data, dataSize));
        image.data = (uint8_t *)stbi_loadf_from_memory((const uint8_t *)data, dataSize, &w, &h, nullptr, STBI_rgb_alpha);
        image.dataSize = w * h * STBI_rgb_alpha * sizeof(float);
        image.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        break;
    default:
        ASSERT(false);
        break;
    }

    ASSERT(image.data);
    image.purpose = purpose;
    image.width = (uint16_t)w;
    image.height = (uint16_t)h;
    image.faceCount = 1;
    image.levelCount = 1;

    if (image.purpose == ImagePurpose::Normal)
        normalizeNormalMap(image);

    return image;
}

void importImage(const uint8_t *data, uint32_t dataSize, ImagePurpose purpose, const char *outImageFilename)
{
    ZoneScoped;
    ASSERT(isValidString(outImageFilename));
    ZoneText(outImageFilename, strlen(outImageFilename));
    Image image = importImage(data, dataSize, purpose);
    writeImage(image, outImageFilename, GenerateMips::Yes, Compress::Yes);
    destroyImage(image);
}

static Image importImage(const char *inImageFilename, ImagePurpose purpose)
{
    ZoneScoped;
    ASSERT(isValidString(inImageFilename));
    ZoneText(inImageFilename, strlen(inImageFilename));
    uint32_t dataSize = readFile(inImageFilename, nullptr, 0);
    uint8_t *data = new uint8_t[dataSize];
    readFile(inImageFilename, data, dataSize);
    Image image = importImage(data, dataSize, purpose);
    delete[] data;
    return image;
}

void importImage(const char *inImageFilename, const char *outImageFilename, ImagePurpose purpose)
{
    ZoneScoped;
    ASSERT(isValidString(inImageFilename));
    ASSERT(isValidString(outImageFilename));
    ZoneText(inImageFilename, strlen(inImageFilename));
    ZoneText(outImageFilename, strlen(outImageFilename));
    Image image = importImage(inImageFilename, purpose);
    writeImage(image, outImageFilename, GenerateMips::Yes, Compress::Yes);
    destroyImage(image);
}

static uint8_t getTexelSize(const Image &image)
{
    switch (image.format)
    {
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_R8G8B8A8_UNORM:
        return 4;
    case VK_FORMAT_R16G16B16A16_SFLOAT:
        return 8;
    case VK_FORMAT_R32G32B32A32_SFLOAT:
        return 16;
    case VK_FORMAT_BC5_UNORM_BLOCK:
    case VK_FORMAT_BC5_SNORM_BLOCK:
    case VK_FORMAT_BC6H_UFLOAT_BLOCK:
    case VK_FORMAT_BC6H_SFLOAT_BLOCK:
    case VK_FORMAT_BC7_UNORM_BLOCK:
    case VK_FORMAT_BC7_SRGB_BLOCK:
        return 1;
    }

    ASSERT(false);
    return 0;
}

static uint32_t calcImagaDataSize(const Image &image)
{
    uint32_t dataSize = 0;
    uint16_t mipWidth = image.width;
    uint16_t mipHeight = image.height;
    uint16_t texelSize = getTexelSize(image);

    for (uint8_t i = 0; i < image.levelCount; i++)
    {
        uint32_t mipSize = mipWidth * mipHeight;

        if (texelSize == 1 && (mipWidth < 4 || mipHeight < 4)) // compressed
        {
            ASSERT(mipWidth == mipHeight && isPowerOf2(mipWidth));
            mipSize = 16;
        }

        dataSize += mipSize;

        if (mipWidth > 1)
            mipWidth >>= 1;
        if (mipHeight > 1)
            mipHeight >>= 1;
    }

    return dataSize * image.faceCount * getTexelSize(image);
}

#ifdef ENABLE_COMPRESSION
struct CompressBlockRowOptions
{
    uint16_t mipWidth;
    uint16_t mipHeight;
    uint16_t rowStride;
    uint16_t blockCountX;
    uint8_t *src;
    uint8_t *dst;
};

static constexpr uint8_t srcBlockSizeInTexels = 4;
static constexpr uint8_t dstBlockSizeInBytes = 16;
static constexpr uint8_t channelCount = 4; // always RGBA textures

static void compressBlockRowBC7(int64_t blockRowIndex, void *userData)
{
    ZoneScoped;
    ASSERT(userData);
    CompressBlockRowOptions &opt = *(CompressBlockRowOptions *)userData;
    uint8_t *srcBlock = opt.src + blockRowIndex * opt.rowStride * srcBlockSizeInTexels;
    uint8_t *dstBlock = opt.dst + blockRowIndex * opt.blockCountX * dstBlockSizeInBytes;

    for (uint32_t x = 0; x < opt.blockCountX; x++)
    {
        CompressBlockBC7(srcBlock, opt.rowStride, dstBlock, optionsBC7);
        srcBlock += srcBlockSizeInTexels * channelCount;
        dstBlock += dstBlockSizeInBytes;
    }
}

static void compressDegenerateBlockBC7(int64_t blockRowIndex, void *userData)
{
    ZoneScoped;
    ASSERT(userData);
    CompressBlockRowOptions &opt = *(CompressBlockRowOptions *)userData;
    uint8_t *srcBlock = opt.src + blockRowIndex * opt.rowStride * srcBlockSizeInTexels;
    uint8_t *dstBlock = opt.dst + blockRowIndex * opt.blockCountX * dstBlockSizeInBytes;
    uint8_t rgba[16][4];
    ASSERT(opt.mipWidth == opt.mipHeight && opt.mipWidth < 4 && isPowerOf2(opt.mipWidth));
    ASSERT(opt.blockCountX == 1 && blockRowIndex == 0);

    for (uint8_t i = 0; i < 16; i++) // copy the first texel to all temp 16 texels
    {
        memcpy(rgba[i], srcBlock, channelCount);
    }

    if (opt.mipWidth == 2) // copy the 3 remaining texels too
    {
        memcpy(rgba[1], srcBlock + 1 * channelCount, channelCount);
        memcpy(rgba[4], srcBlock + 2 * channelCount, channelCount);
        memcpy(rgba[5], srcBlock + 3 * channelCount, channelCount);
    }

    CompressBlockBC7(rgba[0], 4, dstBlock, optionsBC7);
}

static void compressBlockRowBC5(int64_t blockRowIndex, void *userData)
{
    ZoneScoped;
    ASSERT(userData);
    CompressBlockRowOptions &opt = *(CompressBlockRowOptions *)userData;
    const uint8_t *srcBlock = opt.src + blockRowIndex * opt.rowStride * srcBlockSizeInTexels;
    uint8_t *dstBlock = opt.dst + blockRowIndex * opt.blockCountX * dstBlockSizeInBytes;
    uint8_t red[16], green[16]; // deinterleave red and green channels

    for (uint32_t x = 0; x < opt.blockCountX; x++)
    {
        const uint8_t *srcRow = srcBlock;
        for (uint8_t i = 0; i < 4; i++, srcRow += opt.rowStride)
        {
            const uint8_t *srcTexel = srcRow;
            for (uint8_t j = 0; j < 4; j++, srcTexel += channelCount)
            {
                uint32_t index = i * 4 + j;
                red[index] = srcTexel[0];
                green[index] = srcTexel[1];
            }
        }

        CompressBlockBC5(red, 4, green, 4, dstBlock, optionsBC5);
        srcBlock += srcBlockSizeInTexels * channelCount;
        dstBlock += dstBlockSizeInBytes;
    }
}

static void compressDegenerateBlockBC5(int64_t blockRowIndex, void *userData)
{
    ZoneScoped;
    ASSERT(userData);
    CompressBlockRowOptions &opt = *(CompressBlockRowOptions *)userData;
    const uint8_t *srcBlock = opt.src + blockRowIndex * opt.rowStride * srcBlockSizeInTexels;
    uint8_t *dstBlock = opt.dst + blockRowIndex * opt.blockCountX * dstBlockSizeInBytes;
    uint8_t red[16], green[16]; // deinterleave red and green channels
    ASSERT(opt.mipWidth == opt.mipHeight && opt.mipWidth < 4 && isPowerOf2(opt.mipWidth));
    ASSERT(opt.blockCountX == 1 && blockRowIndex == 0);

    for (uint8_t i = 0; i < 16; i++) // copy the first texel to all temp 16 texels
    {
        red[i] = srcBlock[0];
        green[i] = srcBlock[1];
    }

    if (opt.mipWidth == 2) // copy the 3 remaining texels too
    {
        red[1] = (srcBlock + 1 * channelCount)[0];
        green[1] = (srcBlock + 1 * channelCount)[1];
        red[4] = (srcBlock + 2 * channelCount)[0];
        green[4] = (srcBlock + 2 * channelCount)[1];
        red[5] = (srcBlock + 3 * channelCount)[0];
        green[5] = (srcBlock + 3 * channelCount)[1];
    }

    CompressBlockBC5(red, 4, green, 4, dstBlock, optionsBC5);
}

static void compressBlockRowBC6(int64_t blockRowIndex, void *userData)
{
    ZoneScoped;
    ASSERT(userData);
    CompressBlockRowOptions &opt = *(CompressBlockRowOptions *)userData;
    const uint16_t *srcBlock = (const uint16_t *)opt.src + blockRowIndex * opt.rowStride * srcBlockSizeInTexels;
    uint8_t *dstBlock = opt.dst + blockRowIndex * opt.blockCountX * dstBlockSizeInBytes;
    uint16_t rgb[16][3]; // copy red, green and blue channels

    for (uint32_t x = 0; x < opt.blockCountX; x++)
    {
        const uint16_t *srcRow = srcBlock;
        for (uint8_t i = 0; i < 4; i++, srcRow += opt.rowStride)
        {
            const uint16_t *srcTexel = srcRow;
            for (uint8_t j = 0; j < 4; j++, srcTexel += channelCount)
            {
                uint32_t index = i * 4 + j;
                memcpy(rgb[index], srcTexel, 3 * sizeof(uint16_t));
            }
        }

        CompressBlockBC6(rgb[0], 4 * 3, dstBlock, optionsBC6);
        srcBlock += srcBlockSizeInTexels * channelCount;
        dstBlock += dstBlockSizeInBytes;
    }
}

static void compressDegenerateBlockBC6(int64_t blockRowIndex, void *userData)
{
    ZoneScoped;
    ASSERT(userData);
    CompressBlockRowOptions &opt = *(CompressBlockRowOptions *)userData;
    const uint16_t *srcBlock = (const uint16_t *)opt.src + blockRowIndex * opt.rowStride * srcBlockSizeInTexels;
    uint8_t *dstBlock = opt.dst + blockRowIndex * opt.blockCountX * dstBlockSizeInBytes;
    uint16_t rgb[16][3]; // copy red, green and blue channels
    ASSERT(opt.mipWidth == opt.mipHeight && opt.mipWidth < 4 && isPowerOf2(opt.mipWidth));
    ASSERT(opt.blockCountX == 1 && blockRowIndex == 0);

    for (uint8_t i = 0; i < 16; i++) // copy the first texel to all temp 16 texels
    {
        memcpy(rgb[i], srcBlock, 3 * sizeof(uint16_t));
    }

    if (opt.mipWidth == 2) // copy the 3 remaining texels too
    {
        memcpy(rgb[1], srcBlock + 1 * channelCount, 3 * sizeof(uint16_t));
        memcpy(rgb[4], srcBlock + 2 * channelCount, 3 * sizeof(uint16_t));
        memcpy(rgb[5], srcBlock + 3 * channelCount, 3 * sizeof(uint16_t));
    }

    CompressBlockBC6(rgb[0], 4 * 3, dstBlock, optionsBC6);
}

static bool compressImage(const Image &image, Image &compressedImage)
{
    ZoneScoped;
    ASSERT(image.width % srcBlockSizeInTexels == 0 && image.height % srcBlockSizeInTexels == 0);
    VkFormat compressedFormat = VK_FORMAT_UNDEFINED;
    JobFunc compressBlockRowJobFunc = nullptr;
    JobFunc compressDegenerateBlockJobFunc = nullptr; // separate function to avoid unnecessary branching in the general case

    switch (image.purpose)
    {
    case ImagePurpose::Color:
    {
        if (image.format == VK_FORMAT_BC7_SRGB_BLOCK)
            break;

        ASSERT(image.format == VK_FORMAT_R8G8B8A8_SRGB);
        compressedFormat = VK_FORMAT_BC7_SRGB_BLOCK;
        compressBlockRowJobFunc = compressBlockRowBC7;
        compressDegenerateBlockJobFunc = compressDegenerateBlockBC7;
        break;
    }
    case ImagePurpose::Shading:
    {
        if (image.format == VK_FORMAT_BC7_UNORM_BLOCK)
            break;

        ASSERT(image.format == VK_FORMAT_R8G8B8A8_UNORM);
        compressedFormat = VK_FORMAT_BC7_UNORM_BLOCK;
        compressBlockRowJobFunc = compressBlockRowBC7;
        compressDegenerateBlockJobFunc = compressDegenerateBlockBC7;
        break;
    }
    case ImagePurpose::Normal:
    {
        if (image.format == VK_FORMAT_BC5_UNORM_BLOCK)
            break;

        ASSERT(image.format == VK_FORMAT_R8G8B8A8_UNORM);
        compressedFormat = VK_FORMAT_BC5_UNORM_BLOCK;
        compressBlockRowJobFunc = compressBlockRowBC5;
        compressDegenerateBlockJobFunc = compressDegenerateBlockBC5;
        break;
    }
    case ImagePurpose::HDRI:
    {
        if (image.format == VK_FORMAT_BC6H_UFLOAT_BLOCK)
            break;

        ASSERT(image.format == VK_FORMAT_R16G16B16A16_SFLOAT);
        compressedFormat = VK_FORMAT_BC6H_UFLOAT_BLOCK;
        compressBlockRowJobFunc = compressBlockRowBC6;
        compressDegenerateBlockJobFunc = compressDegenerateBlockBC6;
        break;
    }
    default:
        ASSERT(false);
        compressedImage = {};
        return false;
    }

    compressedImage = image;
    compressedImage.format = compressedFormat;
    compressedImage.dataSize = 0;
    uint16_t jobCount = 0;
    uint16_t mipWidth = image.width;
    uint16_t mipHeight = image.height;

    for (uint8_t i = 0; i < image.levelCount; i++)
    {
        uint16_t blockCountX = (uint16_t)aligned(mipWidth, srcBlockSizeInTexels) / srcBlockSizeInTexels;
        uint16_t blockCountY = (uint16_t)aligned(mipHeight, srcBlockSizeInTexels) / srcBlockSizeInTexels;
        compressedImage.dataSize += blockCountX * blockCountY * dstBlockSizeInBytes;
        jobCount += blockCountY;

        if (mipWidth > 1)
            mipWidth >>= 1;
        if (mipHeight > 1)
            mipHeight >>= 1;
    }

    compressedImage.dataSize *= image.faceCount;
    jobCount *= image.faceCount;

    compressedImage.data = new uint8_t[compressedImage.dataSize];
    JobInfo *jobInfos = new JobInfo[jobCount];
    CompressBlockRowOptions *options = new CompressBlockRowOptions[image.levelCount * image.faceCount];

    mipWidth = image.width;
    mipHeight = image.height;
    uint8_t texelSize = getTexelSize(image);
    uint16_t jobIndex = 0;
    uint32_t srcDataOffset = 0;
    uint32_t dstDataOffset = 0;

    for (uint8_t i = 0; i < image.levelCount; i++)
    {
        uint16_t blockCountX = (uint16_t)aligned(mipWidth, srcBlockSizeInTexels) / srcBlockSizeInTexels;
        uint16_t blockCountY = (uint16_t)aligned(mipHeight, srcBlockSizeInTexels) / srcBlockSizeInTexels;

        uint32_t srcMipSize = mipWidth * mipHeight * texelSize;
        uint32_t dstMipSize = blockCountX * blockCountY * dstBlockSizeInBytes;

        for (uint8_t j = 0; j < image.faceCount; j++)
        {
            uint8_t index = i * image.faceCount + j;
            options[index].mipWidth = mipWidth;
            options[index].mipHeight = mipHeight;
            options[index].rowStride = mipWidth * channelCount;
            options[index].blockCountX = blockCountX;
            options[index].src = image.data + srcDataOffset;
            options[index].dst = compressedImage.data + dstDataOffset;
            srcDataOffset += srcMipSize;
            dstDataOffset += dstMipSize;

            if (mipWidth < 4 || mipHeight < 4) // mip is smaller that the min block size
            {
                ASSERT(blockCountX == 1);
                ASSERT(mipWidth == mipHeight && isPowerOf2(mipWidth)); // handle only 1x1 or 2x2 for now
                jobInfos[jobIndex++] = { compressDegenerateBlockJobFunc, 0, &options[index] };
                continue;
            }

            for (uint16_t y = 0; y < blockCountY; y++)
            {
                jobInfos[jobIndex++] = { compressBlockRowJobFunc, y, &options[index] };
            }
        }

        if (mipWidth > 1)
            mipWidth >>= 1;
        if (mipHeight > 1)
            mipHeight >>= 1;
    }

    Token token = createToken();
    enqueueJobs(jobInfos, jobCount, token);
    waitForToken(token);
    destroyToken(token);

    delete[] jobInfos;
    delete[] options;

    return true;
}
#endif // ENABLE_COMPRESSION

uint32_t iterateImageLevelFaces(const Image &image, IterateCallback callback, void *userData)
{
    uint32_t dataOffset = 0;
    uint16_t mipWidth = image.width;
    uint16_t mipHeight = image.height;
    uint8_t texelSize = getTexelSize(image);

    for (uint8_t i = 0; i < image.levelCount; i++)
    {
        uint32_t mipSize = mipWidth * mipHeight * texelSize;

        if (texelSize == 1 && (mipWidth < 4 || mipHeight < 4)) // compressed
        {
            ASSERT(mipWidth == mipHeight && isPowerOf2(mipWidth));
            mipSize = 16;
        }

        for (uint8_t j = 0; j < image.faceCount; j++, dataOffset += mipSize)
        {
            callback(image, i, j, mipWidth, mipHeight, dataOffset, mipSize, userData);
        }

        if (mipWidth > 1)
            mipWidth >>= 1;
        if (mipHeight > 1)
            mipHeight >>= 1;
    }

    return dataOffset;
}

#ifndef MIPS_BLIT
static void generateMipsCompute(GpuImage &gpuImage)
{
    static_assert(false, ""); // TODO
}
#endif // !MIPS_BLIT

void createImageMips(Image &image)
{
    ZoneScoped;
    ASSERT(image.data && image.dataSize);
    ASSERT(isPowerOf2(image.width) && isPowerOf2(image.height));
    ASSERT(image.faceCount == 1 || image.faceCount == 6);
    ASSERT(image.levelCount == 1);
    ASSERT(image.format == VK_FORMAT_R8G8B8A8_SRGB ||
        image.format == VK_FORMAT_R8G8B8A8_UNORM ||
        image.format == VK_FORMAT_R16G16B16A16_SFLOAT);

    image.levelCount = calcMipLevelCount(image.width, image.height);

    VkImageUsageFlags usageFlags = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    QueueFamily queueFamily;
    VkImageLayout imageLayout;
#ifdef MIPS_BLIT
    usageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
    queueFamily = QueueFamily::Graphics;
    imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
#else
    usageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
    queueFamily = QueueFamily::Compute;
    imageLayout = VK_IMAGE_LAYOUT_GENERAL;
#endif
    GpuImage gpuImage = createAndCopyGpuImage(image, queueFamily, usageFlags, image.faceCount == 1 ? GpuImageType::Image2D : GpuImageType::Image2DCubemap, SharingMode::Exclusive, imageLayout);

    Cmd cmd = allocateCmd(queueFamily);
    beginOneTimeCmd(cmd);
#ifdef MIPS_BLIT
    blitGpuImageMips(cmd, gpuImage, imageLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
#else
    generateMipsCompute(gpuImage);
#endif
    endAndSubmitOneTimeCmd(cmd, graphicsQueue, nullptr, nullptr, WaitForFence::Yes);
    freeCmd(cmd);

    copyImage(gpuImage, image, queueFamily);
    destroyGpuImage(gpuImage);
}

void writeImage(Image &image, const char *outImageFilename, GenerateMips generateMips, Compress compress)
{
    ZoneScoped;
    ASSERT(isValidString(outImageFilename));
    ZoneText(outImageFilename, strlen(outImageFilename));
    ASSERT(image.faceCount == 1 || image.faceCount == 6);

    if (generateMips == GenerateMips::Yes && image.levelCount == 1)
        createImageMips(image);

    Image compressedImage = {};

#ifdef ENABLE_COMPRESSION
    if (compress == Compress::Yes)
        compressImage(image, compressedImage);
#else
    UNUSED(compress);
#endif // ENABLE_COMPRESSION

    ktxTexture2 *texture;
    ktxTextureCreateInfo createInfo {};
    createInfo.vkFormat = compressedImage.data ? compressedImage.format : image.format;
    createInfo.baseWidth = image.width;
    createInfo.baseHeight = image.height;
    createInfo.baseDepth = 1;
    createInfo.numDimensions = 2;
    createInfo.numLevels = image.levelCount;
    createInfo.numLayers = 1;
    createInfo.numFaces = image.faceCount;
    createInfo.isArray = false;
    createInfo.generateMipmaps = false;

    VERIFY(ktxTexture2_Create(&createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &texture) == KTX_SUCCESS);

    iterateImageLevelFaces(compressedImage.data ? compressedImage : image,
        [](const Image &image, uint8_t level, uint8_t face, uint16_t mipWidth, uint16_t mipHeight, uint32_t dataOffset, uint32_t dataSize, void *userData)
    {
        UNUSED(mipWidth);
        UNUSED(mipHeight);
        VERIFY(ktxTexture_SetImageFromMemory(ktxTexture(userData), level, 0, face, image.data + dataOffset, dataSize) == KTX_SUCCESS);
    }, texture);

    uint8_t *fileData;
    size_t fileDataSize;
    VERIFY(ktxTexture_WriteToMemory(ktxTexture(texture), &fileData, &fileDataSize) == KTX_SUCCESS);
    ktxTexture_Destroy(ktxTexture(texture));
    writeFile(outImageFilename, fileData, (uint32_t)fileDataSize);
    free(fileData);

    destroyImage(compressedImage);
}

Image loadImage(const char *inImageFilename)
{
    ZoneScoped;
    ASSERT(isValidString(inImageFilename));
    ZoneText(inImageFilename, strlen(inImageFilename));
    ktxTexture2 *texture;
    VERIFY(ktxTexture2_CreateFromNamedFile(inImageFilename, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture) == KTX_SUCCESS);

    Image image {};
    image.width = (uint16_t)texture->baseWidth;
    image.height = (uint16_t)texture->baseHeight;
    image.format = (VkFormat)texture->vkFormat;
    image.faceCount = (uint8_t)texture->numFaces;
    image.levelCount = (uint8_t)texture->numLevels;
    image.dataSize = calcImagaDataSize(image);
    image.data = (uint8_t *)malloc(image.dataSize);

    uint32_t size = iterateImageLevelFaces(image, [](const Image &image, uint8_t level, uint8_t face, uint16_t mipWidth, uint16_t mipHeight, uint32_t dataOffset, uint32_t dataSize, void *userData)
    {
        UNUSED(mipWidth);
        UNUSED(mipHeight);
        ktxTexture2 *texture = (ktxTexture2 *)userData;
        ktx_size_t ktxImageOffset;
        ktxTexture_GetImageOffset(ktxTexture(texture), level, 0, face, &ktxImageOffset);
        memcpy(image.data + dataOffset, texture->pData + ktxImageOffset, dataSize);
    }, texture);
    VERIFY(image.dataSize == size);

    ktxTexture_Destroy(ktxTexture(texture));
    return image;
}

void destroyImage(Image &image)
{
    free(image.data);
    image = {};
}

static void normalizeNormalMap(Image &normalMapImage)
{
    ZoneScoped;
    ASSERT(normalMapImage.data && normalMapImage.dataSize);
    ASSERT(normalMapImage.format == VK_FORMAT_R8G8B8A8_UNORM);
    ASSERT(normalMapImage.purpose == ImagePurpose::Normal);

    GpuImage normalMapGpuImage = createAndCopyGpuImage(normalMapImage, QueueFamily::Compute, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT, GpuImageType::Image2D, SharingMode::Exclusive, VK_IMAGE_LAYOUT_GENERAL);
    setGpuImageName(normalMapGpuImage, NAMEOF(normalMapGpuImage));
    VkDescriptorImageInfo imageInfo { nullptr, normalMapGpuImage.imageView, VK_IMAGE_LAYOUT_GENERAL };
    VkWriteDescriptorSet write = initWriteDescriptorSetImage(commonDescriptorSet, 1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &imageInfo);
    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

    uint32_t workGroupCount = findBestWorkGroupCount2d(max(normalMapImage.width, normalMapImage.height));
    Cmd computeCmd = allocateCmd(QueueFamily::Compute);
    beginOneTimeCmd(computeCmd);
    {
        ScopedGpuZoneAutoCollect(computeCmd, "Normalize normal map");
        ImageBarrier imageBarrier {};
        imageBarrier.image = normalMapGpuImage;
        imageBarrier.srcStageMask = StageFlags::None;
        imageBarrier.dstStageMask = StageFlags::ComputeShader;
        imageBarrier.srcAccessMask = AccessFlags::None;
        imageBarrier.dstAccessMask = AccessFlags::Read;
        imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        pipelineBarrier(computeCmd, nullptr, 0, &imageBarrier, 1);
        vkCmdBindDescriptorSets(computeCmd.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, commonPipelineLayout, 0, 1, &commonDescriptorSet, 0, nullptr);
        vkCmdBindPipeline(computeCmd.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, normalizeNormalMapPipeline);
        vkCmdDispatch(computeCmd.commandBuffer, workGroupCount, workGroupCount, 1);
    }
    endAndSubmitOneTimeCmd(computeCmd, computeQueue, nullptr, nullptr, WaitForFence::Yes);
    freeCmd(computeCmd);

    copyImage(normalMapGpuImage, normalMapImage, QueueFamily::Compute);
    destroyGpuImage(normalMapGpuImage);
}

void computeBrdfLut(const char *brdfLutPath)
{
    ASSERT(isValidString(brdfLutPath));

    GpuImage brdfLutGpuImage = createGpuImage(VK_FORMAT_R16G16B16A16_SFLOAT,
        { brdfLutSize, brdfLutSize }, 1,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    setGpuImageName(brdfLutGpuImage, NAMEOF(brdfLutGpuImage));
    VkDescriptorImageInfo imageInfo { nullptr, brdfLutGpuImage.imageView, VK_IMAGE_LAYOUT_GENERAL };
    VkWriteDescriptorSet write = initWriteDescriptorSetImage(commonDescriptorSet, 1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &imageInfo);
    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

    uint32_t computeBrdfLutWorkGroupCount = findBestWorkGroupCount2d(brdfLutSize);
    Cmd computeCmd = allocateCmd(QueueFamily::Compute);
    beginOneTimeCmd(computeCmd);
    {
        ScopedGpuZoneAutoCollect(computeCmd, "Compute BRDF LUT");
        ImageBarrier imageBarrier {};
        imageBarrier.image = brdfLutGpuImage;
        imageBarrier.srcStageMask = StageFlags::None;
        imageBarrier.dstStageMask = StageFlags::ComputeShader;
        imageBarrier.srcAccessMask = AccessFlags::None;
        imageBarrier.dstAccessMask = AccessFlags::Write;
        imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        pipelineBarrier(computeCmd, nullptr, 0, &imageBarrier, 1);
        vkCmdBindDescriptorSets(computeCmd.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, commonPipelineLayout, 0, 1, &commonDescriptorSet, 0, nullptr);
        vkCmdBindPipeline(computeCmd.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computeBrdfLutPipeline);
        vkCmdDispatch(computeCmd.commandBuffer, computeBrdfLutWorkGroupCount, computeBrdfLutWorkGroupCount, 1);
    }
    endAndSubmitOneTimeCmd(computeCmd, computeQueue, nullptr, nullptr, WaitForFence::Yes);
    freeCmd(computeCmd);

    Image image {};
    image.purpose = ImagePurpose::HDRI;
    copyImage(brdfLutGpuImage, image, QueueFamily::Compute);
    writeImage(image, brdfLutPath, GenerateMips::No, Compress::Yes);
    destroyImage(image);

    destroyGpuImage(brdfLutGpuImage);
}

void computeEnvMaps(const char *hdriPath, const char *skyboxPath, const char *irradianceMapPath, const char *prefilteredMapPath)
{
    ZoneScoped;
    ASSERT(isValidString(hdriPath));
    ASSERT(isValidString(skyboxPath));
    ASSERT(isValidString(irradianceMapPath));
    ASSERT(isValidString(prefilteredMapPath));

    Image image = importImage(hdriPath, ImagePurpose::HDRI);
    GpuImage hdriGpuImage = createAndCopyGpuImage(image, QueueFamily::Compute, VK_IMAGE_USAGE_SAMPLED_BIT);
    setGpuImageName(hdriGpuImage, NAMEOF(hdriGpuImage));
    destroyImage(image);

    uint8_t skyboxLevelCount = calcMipLevelCount(skyboxFaceSize, skyboxFaceSize);
    GpuImage skyboxGpuImage = createGpuImage(VK_FORMAT_R16G16B16A16_SFLOAT,
        { skyboxFaceSize, skyboxFaceSize }, skyboxLevelCount,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        GpuImageType::Image2DCubemap);
    setGpuImageName(skyboxGpuImage, NAMEOF(skyboxGpuImage));
    GpuImage irradianceMapGpuImage = createGpuImage(VK_FORMAT_R16G16B16A16_SFLOAT,
        { irradianceMapFaceSize, irradianceMapFaceSize }, 1,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        GpuImageType::Image2DCubemap);
    setGpuImageName(irradianceMapGpuImage, NAMEOF(irradianceMapGpuImage));
    GpuImage prefilteredMapGpuImage = createGpuImage(VK_FORMAT_R16G16B16A16_SFLOAT,
        { prefilteredMapFaceSize, prefilteredMapFaceSize }, prefilteredMapLevelCount,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        GpuImageType::Image2DCubemap);
    setGpuImageName(prefilteredMapGpuImage, NAMEOF(prefilteredMapGpuImage));

    VkDescriptorImageInfo imageInfos[4 + prefilteredMapLevelCount]
    {
        { nullptr, hdriGpuImage.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
        { nullptr, skyboxGpuImage.imageView, VK_IMAGE_LAYOUT_GENERAL },
        { nullptr, skyboxGpuImage.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
        { nullptr, irradianceMapGpuImage.imageView, VK_IMAGE_LAYOUT_GENERAL }
    };

    VkImageView imageViews[prefilteredMapLevelCount];

    for (uint8_t i = 0; i < prefilteredMapLevelCount; i++)
    {
        VkImageSubresourceRange range = initImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, i, 1);
        VkImageViewCreateInfo imageViewCreateInfo = initImageViewCreateInfo(prefilteredMapGpuImage.image, prefilteredMapGpuImage.format, VK_IMAGE_VIEW_TYPE_CUBE, range);
        vkVerify(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &imageViews[i]));

        imageInfos[4 + i] = { nullptr, imageViews[i], VK_IMAGE_LAYOUT_GENERAL };
    }

    VkWriteDescriptorSet writes[]
    {
        initWriteDescriptorSetImage(commonDescriptorSet, 0, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, imageInfos + 0),
        initWriteDescriptorSetImage(commonDescriptorSet, 1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, imageInfos + 1),
        initWriteDescriptorSetImage(commonDescriptorSet, 2, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, imageInfos + 2),
        initWriteDescriptorSetImage(commonDescriptorSet, 3, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, imageInfos + 3),
        initWriteDescriptorSetImage(commonDescriptorSet, 4, prefilteredMapLevelCount, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, imageInfos + 4)
    };
    vkUpdateDescriptorSets(device, countOf(writes), writes, 0, nullptr);

    VkSemaphore semaphore1, semaphore2;
    VkSemaphoreCreateInfo semaphoreCreateInfo = initSemaphoreCreateInfo();
    vkVerify(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphore1));
    vkVerify(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphore2));
    VkSemaphoreSubmitInfoKHR ownershipReleaseFinishedInfo1 = initSemaphoreSubmitInfo(semaphore1, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR);
    VkSemaphoreSubmitInfoKHR ownershipReleaseFinishedInfo2 = initSemaphoreSubmitInfo(semaphore2, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR);
    ImageBarrier imageBarriers[3] {};

    uint32_t computeSkyboxWorkGroupCount = findBestWorkGroupCount2d(skyboxFaceSize);
    uint32_t computeIrradianceMapWorkGroupCount = findBestWorkGroupCount2d(irradianceMapFaceSize);
    uint32_t computePrefilteredMapWorkGroupCount = findBestWorkGroupCount2d(prefilteredMapFaceSize);

    Cmd computeCmd1 = allocateCmd(QueueFamily::Compute);
    beginOneTimeCmd(computeCmd1);
    {
        ScopedGpuZoneAutoCollect(computeCmd1, "Compute skybox");
        imageBarriers[0].image = skyboxGpuImage;
        imageBarriers[0].srcStageMask = StageFlags::None;
        imageBarriers[0].dstStageMask = StageFlags::ComputeShader;
        imageBarriers[0].srcAccessMask = AccessFlags::None;
        imageBarriers[0].dstAccessMask = AccessFlags::Write;
        imageBarriers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageBarriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageBarriers[1] = imageBarriers[0];
        imageBarriers[1].image = irradianceMapGpuImage;
        imageBarriers[2] = imageBarriers[0];
        imageBarriers[2].image = prefilteredMapGpuImage;
        pipelineBarrier(computeCmd1, nullptr, 0, imageBarriers, 3);
        vkCmdBindDescriptorSets(computeCmd1.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, commonPipelineLayout, 0, 1, &commonDescriptorSet, 0, nullptr);
        vkCmdBindPipeline(computeCmd1.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computeSkyboxPipeline);
        vkCmdDispatch(computeCmd1.commandBuffer, computeSkyboxWorkGroupCount, computeSkyboxWorkGroupCount, 1);
        imageBarriers[0] = {};
        imageBarriers[0].image = skyboxGpuImage;
        imageBarriers[0].srcStageMask = StageFlags::ComputeShader;
        imageBarriers[0].srcAccessMask = AccessFlags::Write;
        imageBarriers[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageBarriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageBarriers[0].srcQueueFamily = QueueFamily::Compute;
        imageBarriers[0].dstQueueFamily = QueueFamily::Graphics;
        pipelineBarrier(computeCmd1, nullptr, 0, imageBarriers, 1);
    }
    endAndSubmitOneTimeCmd(computeCmd1, computeQueue, nullptr, &ownershipReleaseFinishedInfo1, WaitForFence::No);

    Cmd graphicsCmd = allocateCmd(QueueFamily::Graphics);
    beginOneTimeCmd(graphicsCmd);
    {
        ScopedGpuZoneAutoCollect(graphicsCmd, "Mips blit");
        imageBarriers[0] = {};
        imageBarriers[0].image = skyboxGpuImage;
        imageBarriers[0].dstStageMask = StageFlags::Blit;
        imageBarriers[0].dstAccessMask = AccessFlags::Read | AccessFlags::Write;
        imageBarriers[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageBarriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageBarriers[0].srcQueueFamily = QueueFamily::Compute;
        imageBarriers[0].dstQueueFamily = QueueFamily::Graphics;
        pipelineBarrier(graphicsCmd, nullptr, 0, imageBarriers, 1);
        blitGpuImageMips(graphicsCmd, skyboxGpuImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        imageBarriers[0] = {};
        imageBarriers[0].image = skyboxGpuImage;
        imageBarriers[0].srcStageMask = StageFlags::Blit;
        imageBarriers[0].srcAccessMask = AccessFlags::Write;
        imageBarriers[0].srcQueueFamily = QueueFamily::Graphics;
        imageBarriers[0].dstQueueFamily = QueueFamily::Compute;
        pipelineBarrier(graphicsCmd, nullptr, 0, imageBarriers, 1);
    }
    endAndSubmitOneTimeCmd(graphicsCmd, graphicsQueue, &ownershipReleaseFinishedInfo1, &ownershipReleaseFinishedInfo2, WaitForFence::No);

    Cmd computeCmd2 = allocateCmd(QueueFamily::Compute);
    beginOneTimeCmd(computeCmd2);
    {
        ScopedGpuZone(computeCmd2, "Compute irradiance");
        imageBarriers[0] = {};
        imageBarriers[0].image = skyboxGpuImage;
        imageBarriers[0].dstStageMask = StageFlags::ComputeShader;
        imageBarriers[0].dstAccessMask = AccessFlags::Read;
        imageBarriers[0].srcQueueFamily = QueueFamily::Graphics;
        imageBarriers[0].dstQueueFamily = QueueFamily::Compute;
        pipelineBarrier(computeCmd2, nullptr, 0, imageBarriers, 1);
        vkCmdBindDescriptorSets(computeCmd2.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, commonPipelineLayout, 0, 1, &commonDescriptorSet, 0, nullptr);
        vkCmdBindPipeline(computeCmd2.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computeIrradianceMapPipeline);
        vkCmdDispatch(computeCmd2.commandBuffer, computeIrradianceMapWorkGroupCount * 6, computeIrradianceMapWorkGroupCount, 1);
    }
    {
        ScopedGpuZoneAutoCollect(computeCmd2, "Compute prefiltered");
        vkCmdBindPipeline(computeCmd2.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePrefilteredMapPipeline);
        vkCmdPushConstants(computeCmd2.commandBuffer, commonPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(prefilteredMapLevelCount), &prefilteredMapLevelCount);
        vkCmdDispatch(computeCmd2.commandBuffer, computePrefilteredMapWorkGroupCount * 6, computePrefilteredMapWorkGroupCount, 1);
    }
    endAndSubmitOneTimeCmd(computeCmd2, computeQueue, &ownershipReleaseFinishedInfo2, nullptr, WaitForFence::Yes);

    image.purpose = ImagePurpose::HDRI;
    copyImage(skyboxGpuImage, image, QueueFamily::Compute);
    writeImage(image, skyboxPath, GenerateMips::No, Compress::Yes);
    copyImage(irradianceMapGpuImage, image, QueueFamily::Compute);
    writeImage(image, irradianceMapPath, GenerateMips::No, Compress::Yes);
    copyImage(prefilteredMapGpuImage, image, QueueFamily::Compute);
    writeImage(image, prefilteredMapPath, GenerateMips::No, Compress::Yes);
    destroyImage(image);

    destroyGpuImage(hdriGpuImage);
    destroyGpuImage(skyboxGpuImage);
    destroyGpuImage(irradianceMapGpuImage);
    destroyGpuImage(prefilteredMapGpuImage);

    for (uint8_t i = 0; i < prefilteredMapLevelCount; i++)
    {
        vkDestroyImageView(device, imageViews[i], nullptr);
    }

    freeCmd(graphicsCmd);
    freeCmd(computeCmd1);
    freeCmd(computeCmd2);
    vkDestroySemaphore(device, semaphore1, nullptr);
    vkDestroySemaphore(device, semaphore2, nullptr);
}