#include "Graphics.hpp"
#include "DebugUtils.hpp"
#include "ImageUtils.hpp"
#include "JobSystem.hpp"
#include "ShaderUtils.hpp"
#include "Utils.hpp"
#include "VkUtils.hpp"

#include <cmp_core.h>
#define KHRONOS_STATIC
#include <ktx.h>
#define STBI_NO_STDIO
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_HDR
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define TRACY_VK_USE_SYMBOL_TABLE
#include <tracy/Tracy.hpp>
#include <tracy/TracyVulkan.hpp>

#define MIPS_BLIT

extern VkPhysicalDeviceProperties physicalDeviceProperties;
extern VkDevice device;
extern VkDescriptorPool descriptorPool;
extern VmaAllocator allocator;
extern TracyVkCtx tracyContext;

extern VkQueue graphicsQueue;
extern VkQueue computeQueue;

extern VkCommandPool graphicsCommandPool;
extern VkCommandBuffer graphicsCmd;
extern VkCommandPool computeCommandPool;
extern VkCommandBuffer computeCmd;

static VkPipelineLayout generateSkyboxPipelineLayout;
static VkPipeline generateSkyboxPipeline;
static VkDescriptorSetLayout generateSkyboxDescriptorSetLayout;
static VkDescriptorSet generateSkyboxDescriptorSet;
static VkSampler linearClampSampler;
static uint32_t generateSkyboxWorkGroupSize, generateSkyboxWorkGroupCount;
static constexpr uint32_t skyboxSize = 2048;

static constexpr float defaultCompressionQuality = 0.1f;
static void *optionsBC5;
static void *optionsBC6;
static void *optionsBC7;

static void pickBestWorkGroupSize2d(uint32_t workSize, uint32_t &workGroupSize, uint32_t &workGroupCount)
{
    workGroupSize = min(physicalDeviceProperties.limits.maxComputeWorkGroupSize[0], physicalDeviceProperties.limits.maxComputeWorkGroupSize[1]);
    uint32_t maxInvocations = physicalDeviceProperties.limits.maxComputeWorkGroupInvocations;
    workGroupCount = (workSize + workGroupSize - 1) / workGroupSize;

    while (maxInvocations < workGroupSize * workGroupSize)
    {
        workGroupSize /= 2;
        workGroupCount *= 2;
    }
}

void initImageUtils(const char *generateSkyboxShaderPath, const char *generateMipsShaderPath)
{
    UNUSED(generateMipsShaderPath);

    VkSamplerCreateInfo samplerCreateInfo = initSamplerCreateInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);
    vkVerify(vkCreateSampler(device, &samplerCreateInfo, nullptr, &linearClampSampler));

    VkDescriptorSetLayoutBinding setBindings[]
    {
        {0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},
        {2, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, &linearClampSampler}
    };

    VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo = initDescriptorSetLayoutCreateInfo(setBindings, COUNTOF(setBindings));
    vkVerify(vkCreateDescriptorSetLayout(device, &setLayoutCreateInfo, nullptr, &generateSkyboxDescriptorSetLayout));
    VkDescriptorSetAllocateInfo setAllocateInfo = initDescriptorSetAllocateInfo(descriptorPool, &generateSkyboxDescriptorSetLayout, 1);
    vkVerify(vkAllocateDescriptorSets(device, &setAllocateInfo, &generateSkyboxDescriptorSet));

    pickBestWorkGroupSize2d(skyboxSize, generateSkyboxWorkGroupSize, generateSkyboxWorkGroupCount);
    VkSpecializationMapEntry specEntries[]
    {
        {0, 0, sizeof(uint32_t)}
    };
    uint32_t specData[] { generateSkyboxWorkGroupSize };
    VkSpecializationInfo specInfo { COUNTOF(specEntries), specEntries,  sizeof(specData), specData };
    VkShaderModule generateSkyboxShader = createShaderModuleFromSpv(device, generateSkyboxShaderPath);
    VkPipelineShaderStageCreateInfo shaderStageCreateInfo = initPipelineShaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT, generateSkyboxShader, &specInfo);
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = initPipelineLayoutCreateInfo(&generateSkyboxDescriptorSetLayout, 1);
    vkVerify(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &generateSkyboxPipelineLayout));
    VkComputePipelineCreateInfo computePipelineCreateInfo = initComputePipelineCreateInfo(shaderStageCreateInfo, generateSkyboxPipelineLayout);
    vkVerify(vkCreateComputePipelines(device, nullptr, 1, &computePipelineCreateInfo, nullptr, &generateSkyboxPipeline));

    vkDestroyShaderModule(device, generateSkyboxShader, nullptr);

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
    vkDestroyDescriptorSetLayout(device, generateSkyboxDescriptorSetLayout, nullptr);
    vkDestroyPipelineLayout(device, generateSkyboxPipelineLayout, nullptr);
    vkDestroyPipeline(device, generateSkyboxPipeline, nullptr);

    DestroyOptionsBC5(optionsBC5);
    DestroyOptionsBC5(optionsBC6);
    DestroyOptionsBC5(optionsBC7);
}

Image importImage(const uint8_t *data, uint32_t dataSize, ImagePurpose purpose)
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

    return image;
}

Image importImage(const char *inImageFilename, ImagePurpose purpose)
{
    ZoneScoped;
    ASSERT(inImageFilename && *inImageFilename);
    ZoneText(inImageFilename, strlen(inImageFilename));
    uint32_t dataSize = readFile(inImageFilename, nullptr, 0);
    uint8_t *data = new uint8_t[dataSize];
    readFile(inImageFilename, data, dataSize);
    Image image = importImage(data, dataSize, purpose);
    delete[] data;
    return image;
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

#ifdef MIPS_BLIT
static void generateMipsBlit(GpuImage &gpuImage, uint8_t levelCount, uint8_t layerCount)
{
    ImageBarrier imageBarriers[2] {};
    imageBarriers[0].image = gpuImage.image;
    imageBarriers[0].srcStageMask = StageFlags::Blit;
    imageBarriers[0].dstStageMask = StageFlags::Blit;
    imageBarriers[0].srcAccessMask = AccessFlags::Write;
    imageBarriers[0].dstAccessMask = AccessFlags::Read;
    imageBarriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageBarriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    imageBarriers[0].levelCount = 1;
    imageBarriers[0].baseArrayLayer = 0;
    imageBarriers[0].layerCount = layerCount;

    VkImageBlit blit {};
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.baseArrayLayer = 0;
    blit.srcSubresource.layerCount = layerCount;
    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.baseArrayLayer = 0;
    blit.dstSubresource.layerCount = layerCount;

    int32_t mipWidth = gpuImage.imageExtent.width;
    int32_t mipHeight = gpuImage.imageExtent.height;

    beginOneTimeCmd(graphicsCmd, graphicsCommandPool);
    {
        TracyVkZone(tracyContext, graphicsCmd, "Mips Blit");
        for (uint8_t i = 1; i < levelCount; i++)
        {
            imageBarriers[0].baseMipLevel = i - 1;
            pipelineBarrier(graphicsCmd, nullptr, 0, imageBarriers, 1);

            blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
            blit.srcSubresource.mipLevel = i - 1;

            if (mipWidth > 1)
                mipWidth >>= 1;
            if (mipHeight > 1)
                mipHeight >>= 1;

            blit.dstOffsets[1] = { mipWidth, mipHeight, 1 };
            blit.dstSubresource.mipLevel = i;

            vkCmdBlitImage(graphicsCmd,
                gpuImage.image,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                gpuImage.image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &blit,
                VK_FILTER_LINEAR);
        }

        imageBarriers[0].baseMipLevel = levelCount - 1;
        imageBarriers[1].image = gpuImage.image;
        imageBarriers[1].srcStageMask = StageFlags::Blit;
        imageBarriers[1].dstStageMask = StageFlags::None;
        imageBarriers[1].srcAccessMask = AccessFlags::Write;
        imageBarriers[1].dstAccessMask = AccessFlags::None;
        imageBarriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        imageBarriers[1].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        pipelineBarrier(graphicsCmd, nullptr, 0, imageBarriers, 2);
    }
    TracyVkCollect(tracyContext, graphicsCmd);
    endAndSubmitOneTimeCmd(graphicsCmd, graphicsQueue, nullptr, nullptr, WaitForFence::Yes);
}
#else
static void generateMipsCompute(GpuImage &gpuImage, uint32_t levelCount, uint8_t layerCount)
{
    static_assert(false, ""); // TODO
}
#endif // MIPS_BLIT

static void generateImageMips(Image &image)
{
    ZoneScoped;
    ASSERT(image.data && image.dataSize);
    ASSERT(isPowerOf2(image.width) && isPowerOf2(image.height));
    ASSERT(image.faceCount == 1 || image.faceCount == 6);
    ASSERT(image.levelCount == 1);
    ASSERT(image.format == VK_FORMAT_R8G8B8A8_SRGB ||
        image.format == VK_FORMAT_R8G8B8A8_UNORM ||
        image.format == VK_FORMAT_R16G16B16A16_SFLOAT);

    image.levelCount = (uint8_t)(log2f((float)max(image.width, image.height))) + 1;

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
    GpuImage gpuImage;

    if (image.faceCount == 1)
        gpuImage = createGpuImage2D(image.format, { image.width, image.height }, image.levelCount, usageFlags);
    else
        gpuImage = createGpuImage2DArray(image.format, { image.width, image.height }, image.levelCount, usageFlags, Cubemap::No);

    memcpy(stagingBuffer.mappedData, image.data, image.dataSize);
    VkBufferImageCopy copyRegion {};
    copyRegion.imageExtent = gpuImage.imageExtent;
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.layerCount = image.faceCount;
    copyStagingBufferToImage(gpuImage.image, &copyRegion, 1, queueFamily, imageLayout);

#ifdef MIPS_BLIT
    generateMipsBlit(gpuImage, image.levelCount, image.faceCount);
#else
    generateMipsCompute(gpuImage, image.levelCount, image.faceCount);
#endif

    uint8_t copyRegionsCount = image.faceCount * image.levelCount;
    VkBufferImageCopy *copyRegions = (VkBufferImageCopy *)alloca(copyRegionsCount * sizeof(VkBufferImageCopy));
    memset(copyRegions, 0, copyRegionsCount * sizeof(VkBufferImageCopy));

    image.dataSize = iterateImageLevelFaces(image, [](const Image &image, uint8_t level, uint8_t face, uint16_t mipWidth, uint16_t mipHeight, uint32_t dataOffset, uint32_t dataSize, void *userData)
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

    copyImageToStagingBuffer(gpuImage.image, copyRegions, copyRegionsCount, queueFamily);
    destroyGpuImage(gpuImage);

    delete[] image.data;
    image.data = new uint8_t[image.dataSize];
    memcpy(image.data, stagingBuffer.mappedData, image.dataSize);
}

void writeImage(Image &image, const char *outImageFilename, GenerateMips generateMips, Compress compress)
{
    ZoneScoped;
    ASSERT(outImageFilename && *outImageFilename);
    ZoneText(outImageFilename, strlen(outImageFilename));
    ASSERT(image.faceCount == 1 || image.faceCount == 6);
    ASSERT(image.levelCount == 1);

    if (generateMips == GenerateMips::Yes)
        generateImageMips(image);

    Image compressedImage = {};

    if (compress == Compress::Yes)
        compressImage(image, compressedImage);

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

    freeImage(compressedImage);
}

Image loadImage(const char *inImageFilename)
{
    ZoneScoped;
    ASSERT(inImageFilename && *inImageFilename);
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

void importImage(const char *inImageFilename, const char *outImageFilename, ImagePurpose purpose)
{
    ZoneScoped;
    ASSERT(inImageFilename && *inImageFilename);
    ASSERT(outImageFilename && *outImageFilename);
    ZoneText(inImageFilename, strlen(inImageFilename));
    ZoneText(outImageFilename, strlen(outImageFilename));
    Image image = importImage(inImageFilename, purpose);
    writeImage(image, outImageFilename, GenerateMips::Yes, Compress::Yes);
    freeImage(image);
}

void freeImage(Image &image)
{
    free(image.data);
    image = {};
}

void generateSkybox(const char *hdriPath, const char *skyboxPath)
{
    ZoneScoped;
    Image image = importImage(hdriPath, ImagePurpose::HDRI);
    GpuImage hdriGpuImage = createAndUploadGpuImage2D(image, QueueFamily::Compute, VK_IMAGE_USAGE_SAMPLED_BIT);
    freeImage(image);

    GpuImage skyboxGpuImage = createGpuImage2DArray(VK_FORMAT_R16G16B16A16_SFLOAT,
        { skyboxSize, skyboxSize }, 1,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        Cubemap::No);

    VkDescriptorImageInfo imageInfos[2]
    {
        { nullptr, hdriGpuImage.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
        { nullptr, skyboxGpuImage.imageView, VK_IMAGE_LAYOUT_GENERAL }
    };
    VkWriteDescriptorSet writes[2]
    {
        initWriteDescriptorSetImage(generateSkyboxDescriptorSet, 0, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, imageInfos + 0),
        initWriteDescriptorSetImage(generateSkyboxDescriptorSet, 1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, imageInfos + 1)
    };
    vkUpdateDescriptorSets(device, COUNTOF(writes), writes, 0, nullptr);

    beginOneTimeCmd(computeCmd, computeCommandPool);
    {
        TracyVkZone(tracyContext, computeCmd, "Generate Skybox");
        ImageBarrier imageBarrier {};
        imageBarrier.image = skyboxGpuImage.image;
        imageBarrier.srcStageMask = StageFlags::None;
        imageBarrier.dstStageMask = StageFlags::ComputeShader;
        imageBarrier.srcAccessMask = AccessFlags::None;
        imageBarrier.dstAccessMask = AccessFlags::Write;
        imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        pipelineBarrier(computeCmd, nullptr, 0, &imageBarrier, 1);
        vkCmdBindPipeline(computeCmd, VK_PIPELINE_BIND_POINT_COMPUTE, generateSkyboxPipeline);
        vkCmdBindDescriptorSets(computeCmd, VK_PIPELINE_BIND_POINT_COMPUTE, generateSkyboxPipelineLayout, 0, 1, &generateSkyboxDescriptorSet, 0, nullptr);
        vkCmdDispatch(computeCmd, generateSkyboxWorkGroupCount, generateSkyboxWorkGroupCount, 1);
    }
    TracyVkCollect(tracyContext, computeCmd);
    endAndSubmitOneTimeCmd(computeCmd, computeQueue, nullptr, nullptr, WaitForFence::Yes);

    uint32_t skyboxImageLayerSize = skyboxSize * skyboxSize * 4 * sizeof(uint16_t);
    VkBufferImageCopy copyRegions[6] {};
    for (uint8_t i = 0; i < COUNTOF(copyRegions); i++)
    {
        copyRegions[i].bufferOffset = i * skyboxImageLayerSize;
        copyRegions[i].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegions[i].imageSubresource.mipLevel = 0;
        copyRegions[i].imageSubresource.baseArrayLayer = i;
        copyRegions[i].imageSubresource.layerCount = 1;
        copyRegions[i].imageExtent = { skyboxSize, skyboxSize, 1 };
    }
    copyImageToStagingBuffer(skyboxGpuImage.image, copyRegions, COUNTOF(copyRegions), QueueFamily::Compute);

    image = {};
    image.dataSize = skyboxImageLayerSize * 6;
    image.faceCount = 6;
    image.levelCount = 1;
    image.width = skyboxSize;
    image.height = skyboxSize;
    image.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    image.purpose = ImagePurpose::HDRI;
    image.data = new unsigned char[image.dataSize];
    memcpy(image.data, stagingBuffer.mappedData, image.dataSize);
    writeImage(image, skyboxPath, GenerateMips::Yes, Compress::Yes);
    delete[] image.data;

    destroyGpuImage(hdriGpuImage);
    destroyGpuImage(skyboxGpuImage);
}