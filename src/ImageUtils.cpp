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
#include <tracy/Tracy.hpp>

extern VkPhysicalDeviceProperties physicalDeviceProperties;
extern VkDevice device;
extern VkDescriptorPool descriptorPool;
extern VmaAllocator allocator;

extern VkQueue computeQueue;
extern uint32_t computeQueueFamilyIndex;

static VkCommandPool computeCommandPool;
static VkCommandBuffer computeCmd;

static VkSampler linearClampSampler;
static const char *skyboxShaderPath;
static const char *mipsShaderPath;

static constexpr float defaultCompressionQuality = 0.1f;
static void *optionsBC5;
static void *optionsBC6;
static void *optionsBC7;

void initImageUtils(const char *generateSkyboxShaderPath, const char *generateMipsShaderPath)
{
    skyboxShaderPath = generateSkyboxShaderPath;
    mipsShaderPath = generateMipsShaderPath;

    VkCommandPoolCreateInfo commandPoolCreateInfo = initCommandPoolCreateInfo(computeQueueFamilyIndex);
    vkVerify(vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &computeCommandPool));
    VkCommandBufferAllocateInfo commandBufferAllocateInfo = initCommandBufferAllocateInfo(computeCommandPool, 1);
    vkVerify(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &computeCmd));

    VkSamplerCreateInfo samplerCreateInfo = initSamplerCreateInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);
    vkVerify(vkCreateSampler(device, &samplerCreateInfo, nullptr, &linearClampSampler));

    CreateOptionsBC5(&optionsBC5);
    SetQualityBC5(optionsBC5, defaultCompressionQuality);
    CreateOptionsBC6(&optionsBC6);
    SetQualityBC6(optionsBC6, defaultCompressionQuality);
    CreateOptionsBC7(&optionsBC7);
    SetQualityBC7(optionsBC7, defaultCompressionQuality);
}

void terminateImageUtils()
{
    vkDestroyCommandPool(device, computeCommandPool, nullptr);
    vkDestroySampler(device, linearClampSampler, nullptr);

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
        image.faceSize = w * h * STBI_rgb_alpha;
        image.format = VK_FORMAT_R8G8B8A8_SRGB;
        break;
    case ImagePurpose::Normal:
        image.data = stbi_load_from_memory(data, dataSize, &w, &h, nullptr, STBI_rgb);
        image.faceSize = w * h * STBI_rgb;
        image.format = VK_FORMAT_R8G8B8_UNORM;
        break;
    case ImagePurpose::Shading:
        image.data = stbi_load_from_memory(data, dataSize, &w, &h, nullptr, STBI_rgb_alpha);
        image.faceSize = w * h * STBI_rgb_alpha;
        image.format = VK_FORMAT_R8G8B8A8_UNORM;
        break;
    case ImagePurpose::HDRI:
        ASSERT(stbi_is_hdr_from_memory((const uint8_t *)data, dataSize));
        image.data = (uint8_t *)stbi_loadf_from_memory((const uint8_t *)data, dataSize, &w, &h, nullptr, STBI_rgb_alpha);
        image.faceSize = w * h * STBI_rgb_alpha * sizeof(float);
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

struct CompressBlockRowOptions
{
    uint16_t blockCountX;
    uint16_t imageWidth;
    uint8_t channelCount;
    uint8_t *src;
    uint8_t *dst;
};

static constexpr uint8_t srcBlockSizeInTexels = 4;
static constexpr uint8_t dstBlockSizeInBytes = 16;

static void compressBlockRowBC7(int64_t rowIndex, void *userData)
{
    ZoneScoped;
    ASSERT(userData);
    CompressBlockRowOptions &opt = *(CompressBlockRowOptions *)userData;
    const uint32_t stride = opt.imageWidth * opt.channelCount;
    const uint8_t *srcBlock = opt.src + rowIndex * stride * srcBlockSizeInTexels;
    uint8_t *dstBlock = opt.dst + rowIndex * opt.blockCountX * dstBlockSizeInBytes;

    for (uint32_t x = 0; x < opt.blockCountX; x++)
    {
        CompressBlockBC7(srcBlock, stride, dstBlock, optionsBC7);
        srcBlock += srcBlockSizeInTexels * opt.channelCount;
        dstBlock += dstBlockSizeInBytes;
    }
}

static void compressBlockRowBC5CopyChannels(int64_t rowIndex, void *userData)
{
    ZoneScoped;
    ASSERT(userData);
    CompressBlockRowOptions &opt = *(CompressBlockRowOptions *)userData;
    const uint32_t stride = opt.imageWidth * opt.channelCount;
    const uint8_t *srcBlock = opt.src + rowIndex * stride * srcBlockSizeInTexels;
    uint8_t *dstBlock = opt.dst + rowIndex * opt.blockCountX * dstBlockSizeInBytes;
    uint8_t red[16], green[16]; // deinterleave red and green channels

    for (uint32_t x = 0; x < opt.blockCountX; x++)
    {
        const uint8_t *srcRow = srcBlock;
        for (uint8_t i = 0; i < 4; i++, srcRow += stride)
        {
            const uint8_t *srcTexel = srcRow;
            for (uint8_t j = 0; j < 4; j++, srcTexel += opt.channelCount)
            {
                uint32_t index = i * 4 + j;
                red[index] = srcTexel[0];
                green[index] = srcTexel[1];
            }
        }

        CompressBlockBC5(red, 4, green, 4, dstBlock, optionsBC5);
        srcBlock += srcBlockSizeInTexels * opt.channelCount;
        dstBlock += dstBlockSizeInBytes;
    }
}

static void compressBlockRowBC6CopyChannels(int64_t blockRowIndex, void *userData)
{
    ZoneScoped;
    ASSERT(userData);
    CompressBlockRowOptions &opt = *(CompressBlockRowOptions *)userData;
    const uint32_t stride = opt.imageWidth * opt.channelCount;
    const uint16_t *srcBlock = (const uint16_t *)opt.src + blockRowIndex * stride * srcBlockSizeInTexels;
    uint8_t *dstBlock = opt.dst + blockRowIndex * opt.blockCountX * dstBlockSizeInBytes;
    uint16_t rgb[16][3]; // copy red, green and blue channels

    for (uint32_t x = 0; x < opt.blockCountX; x++)
    {
        const uint16_t *srcRow = srcBlock;
        for (uint8_t i = 0; i < 4; i++, srcRow += stride)
        {
            const uint16_t *srcTexel = srcRow;
            for (uint8_t j = 0; j < 4; j++, srcTexel += opt.channelCount)
            {
                uint32_t index = i * 4 + j;
                memcpy(rgb[index], srcTexel, 3 * sizeof(uint16_t));
            }
        }

        CompressBlockBC6(rgb[0], 4 * 3, dstBlock, optionsBC6);
        srcBlock += srcBlockSizeInTexels * opt.channelCount;
        dstBlock += dstBlockSizeInBytes;
    }
}

void writeImage(Image &image, const char *outImageFilename)
{
    ZoneScoped;
    ASSERT(outImageFilename && *outImageFilename);
    ZoneText(outImageFilename, strlen(outImageFilename));
    ASSERT(image.width % srcBlockSizeInTexels == 0 && image.height % srcBlockSizeInTexels == 0);
    ASSERT(image.faceCount == 1 || image.faceCount == 6);
    ASSERT(image.levelCount == 1);

    VkFormat dstFormat = VK_FORMAT_UNDEFINED;
    uint8_t channelCount = 0;
    JobFunc jobFunc = nullptr;

    switch (image.purpose)
    {
    case ImagePurpose::Color:
    case ImagePurpose::Shading:
    {
        if (image.format == VK_FORMAT_BC7_SRGB_BLOCK || image.format == VK_FORMAT_BC7_UNORM_BLOCK)
            break;

        ASSERT(image.format == VK_FORMAT_R8G8B8A8_SRGB || image.format == VK_FORMAT_R8G8B8A8_UNORM);
        dstFormat = image.purpose == ImagePurpose::Color ? VK_FORMAT_BC7_SRGB_BLOCK : image.purpose == ImagePurpose::Shading ? VK_FORMAT_BC7_UNORM_BLOCK : VK_FORMAT_UNDEFINED;
        channelCount = 4;
        jobFunc = compressBlockRowBC7;
        break;
    }
    case ImagePurpose::Normal:
    {
        if (image.format == VK_FORMAT_BC5_UNORM_BLOCK)
            break;

        ASSERT(image.format == VK_FORMAT_R8G8B8_UNORM || image.format == VK_FORMAT_R8G8B8A8_UNORM);
        dstFormat = VK_FORMAT_BC5_UNORM_BLOCK;
        channelCount = image.format == VK_FORMAT_R8G8B8_UNORM ? 3 : image.format == VK_FORMAT_R8G8B8A8_UNORM ? 4 : 0;
        jobFunc = compressBlockRowBC5CopyChannels;
        break;
    }
    case ImagePurpose::HDRI:
    {
        if (image.format == VK_FORMAT_BC6H_UFLOAT_BLOCK)
            break;

        ASSERT(image.format == VK_FORMAT_R16G16B16_SFLOAT || image.format == VK_FORMAT_R16G16B16A16_SFLOAT);
        dstFormat = VK_FORMAT_BC6H_UFLOAT_BLOCK;
        channelCount = image.format == VK_FORMAT_R16G16B16_SFLOAT ? 3 : image.format == VK_FORMAT_R16G16B16A16_SFLOAT ? 4 : 0;
        jobFunc = compressBlockRowBC6CopyChannels;
        break;
    }
    default:
        ASSERT(false);
        return;
    }

    uint32_t dstDataFaceSize = 0;
    uint8_t *dstData = nullptr;

    if (dstFormat)
    {
        ASSERT(channelCount);
        const uint16_t blockCountX = image.width / srcBlockSizeInTexels, blockCountY = image.height / srcBlockSizeInTexels;
        dstDataFaceSize = blockCountX * blockCountY * dstBlockSizeInBytes;
        dstData = new uint8_t[image.faceCount * dstDataFaceSize];
        CompressBlockRowOptions *options = (CompressBlockRowOptions *)alloca(image.faceCount * sizeof(CompressBlockRowOptions));
        JobInfo *jobInfos = new JobInfo[blockCountY];
        Token token = createToken();

        for (uint8_t i = 0; i < image.faceCount; i++)
        {
            options[i].blockCountX = blockCountX;
            options[i].imageWidth = image.width;
            options[i].channelCount = channelCount;
            options[i].src = image.data + i * image.faceSize;
            options[i].dst = dstData + i * dstDataFaceSize;

            for (uint32_t y = 0; y < blockCountY; y++)
            {
                jobInfos[y] = { jobFunc, y, &options[i] };
            }

            enqueueJobs(jobInfos, blockCountY, token);
        }

        delete[] jobInfos;
        waitForToken(token);
        destroyToken(token);
    }

    ASSERT((!dstData && !dstFormat) || (dstData && dstFormat));
    ktxTexture2 *texture;
    ktxTextureCreateInfo createInfo {};
    createInfo.vkFormat = dstData ? dstFormat : image.format;
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

    for (uint8_t i = 0; i < image.faceCount; i++)
    {
        const uint8_t *data = dstData ? dstData + i * dstDataFaceSize : image.data + i * image.faceSize;
        const uint32_t size = dstData ? dstDataFaceSize : image.faceSize;
        VERIFY(ktxTexture_SetImageFromMemory(ktxTexture(texture), 0, 0, i, data, size) == KTX_SUCCESS);
    }

    uint8_t *fileData;
    size_t fileDataSize;
    VERIFY(ktxTexture_WriteToMemory(ktxTexture(texture), &fileData, &fileDataSize) == KTX_SUCCESS);
    ktxTexture_Destroy(ktxTexture(texture));
    writeFile(outImageFilename, fileData, (uint32_t)fileDataSize);
    free(fileData);

    delete[] dstData;
}

static ktx_error_code_e iterateCallback(int miplevel, int face,
    int width, int height, int depth,
    ktx_uint64_t faceLodSize,
    void *pixels, void *userdata)
{
    UNUSED(miplevel);
    UNUSED(width);
    UNUSED(height);
    UNUSED(depth);
    ASSERT(userdata);
    Image &image = *(Image *)userdata;
    ASSERT(image.faceSize == faceLodSize);
    memcpy(image.data + face * faceLodSize, pixels, faceLodSize);
    return KTX_SUCCESS;
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
    image.faceSize = (uint32_t)ktxTexture_GetImageSize(ktxTexture(texture), 0);
    image.data = (uint8_t *)malloc(image.faceCount * image.faceSize);

    ktxTexture_IterateLevelFaces(ktxTexture(texture), iterateCallback, &image);
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
    writeImage(image, outImageFilename);
    freeImage(image);
}

void freeImage(Image &image)
{
    free(image.data);
    image = {};
}

void generateSkybox(const char *hdriPath, const char *skyboxPath)
{
    static constexpr uint32_t skyboxSize = 2048;
    ZoneScoped;
    VkDescriptorSetLayoutBinding setBindings[]
    {
        {0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},
        {2, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, &linearClampSampler}
    };
    VkDescriptorSetLayout setLayout;
    VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo = initDescriptorSetLayoutCreateInfo(setBindings, COUNTOF(setBindings));
    vkVerify(vkCreateDescriptorSetLayout(device, &setLayoutCreateInfo, nullptr, &setLayout));
    VkDescriptorSet set;
    VkDescriptorSetAllocateInfo setAllocateInfo = initDescriptorSetAllocateInfo(descriptorPool, &setLayout, 1);
    vkVerify(vkAllocateDescriptorSets(device, &setAllocateInfo, &set));

    uint32_t workGroupSize = min(physicalDeviceProperties.limits.maxComputeWorkGroupSize[0], physicalDeviceProperties.limits.maxComputeWorkGroupSize[1]);
    uint32_t maxInvocations = physicalDeviceProperties.limits.maxComputeWorkGroupInvocations;
    uint32_t workGroupCount = (skyboxSize + workGroupSize - 1) / workGroupSize;

    while (maxInvocations < workGroupSize * workGroupSize)
    {
        workGroupSize /= 2;
        workGroupCount *= 2;
    }

    VkSpecializationMapEntry specEntries[]
    {
        {0, 0, sizeof(uint32_t)}
    };
    uint32_t specData[] { workGroupSize };
    VkSpecializationInfo specInfo { COUNTOF(specEntries), specEntries,  sizeof(specData), specData };
    VkShaderModule generateSkyboxShader = createShaderModuleFromSpv(device, skyboxShaderPath);
    VkPipelineShaderStageCreateInfo shaderStageCreateInfo = initPipelineShaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT, generateSkyboxShader, &specInfo);

    VkPipelineLayout generateSkyboxPipelineLayout;
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = initPipelineLayoutCreateInfo(&setLayout, 1);
    vkVerify(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &generateSkyboxPipelineLayout));

    VkPipeline generateSkyboxPipeline;
    VkComputePipelineCreateInfo computePipelineCreateInfo = initComputePipelineCreateInfo(shaderStageCreateInfo, generateSkyboxPipelineLayout);
    vkVerify(vkCreateComputePipelines(device, nullptr, 1, &computePipelineCreateInfo, nullptr, &generateSkyboxPipeline));

    Image image = importImage(hdriPath, ImagePurpose::HDRI);
    memcpy((char *)stagingBuffer.mappedData, image.data, image.faceSize);
    GpuImage hdriGpuImage = createGpuImage2D(image.format,
        { image.width, image.height },
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT);
    freeImage(image);

    VkBufferImageCopy copyRegion {};
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent = hdriGpuImage.imageExtent;
    copyStagingBufferToImage(hdriGpuImage.image, &copyRegion, 1, QueueFamily::Compute);

    GpuImage skyboxGpuImage = createGpuImage2DArray(VK_FORMAT_R16G16B16A16_SFLOAT,
        { skyboxSize, skyboxSize },
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        false);

    VkDescriptorImageInfo imageInfos[2]
    {
        { nullptr, hdriGpuImage.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
        { nullptr, skyboxGpuImage.imageView, VK_IMAGE_LAYOUT_GENERAL }
    };
    VkWriteDescriptorSet writes[2]
    {
        initWriteDescriptorSetImage(set, 0, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, imageInfos + 0),
        initWriteDescriptorSetImage(set, 1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, imageInfos + 1)
    };
    vkUpdateDescriptorSets(device, COUNTOF(writes), writes, 0, nullptr);

    beginOneTimeCmd(computeCmd, computeCommandPool);
    ImageBarrier imageBarrier {};
    imageBarrier.image = skyboxGpuImage.image;
    imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE_KHR;
    imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR;
    imageBarrier.srcAccessMask = AccessFlags::None;
    imageBarrier.dstAccessMask = AccessFlags::Write;
    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    pipelineBarrier(computeCmd, nullptr, 0, &imageBarrier, 1);
    vkCmdBindPipeline(computeCmd, VK_PIPELINE_BIND_POINT_COMPUTE, generateSkyboxPipeline);
    vkCmdBindDescriptorSets(computeCmd, VK_PIPELINE_BIND_POINT_COMPUTE, generateSkyboxPipelineLayout, 0, 1, &set, 0, nullptr);
    vkCmdDispatch(computeCmd, workGroupCount, workGroupCount, 1);
    endAndSubmitOneTimeCmd(computeCmd, computeQueue, nullptr, nullptr, true);

    uint32_t skyboxImageLayerSize = skyboxSize * skyboxSize * 4 * sizeof(uint16_t);
    VkBufferImageCopy copyRegions[6] {};
    for (uint8_t j = 0; j < COUNTOF(copyRegions); j++)
    {
        copyRegions[j].bufferOffset = j * skyboxImageLayerSize;
        copyRegions[j].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegions[j].imageSubresource.mipLevel = 0;
        copyRegions[j].imageSubresource.baseArrayLayer = j;
        copyRegions[j].imageSubresource.layerCount = 1;
        copyRegions[j].imageExtent = { skyboxSize, skyboxSize, 1 };
    }
    copyImageToStagingBuffer(skyboxGpuImage.image, copyRegions, COUNTOF(copyRegions), QueueFamily::Compute);

    image = {};
    image.faceSize = skyboxImageLayerSize;
    image.faceCount = 6;
    image.levelCount = 1;
    image.width = skyboxSize;
    image.height = skyboxSize;
    image.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    image.purpose = ImagePurpose::HDRI;
    image.data = new unsigned char[image.faceCount * image.faceSize];
    memcpy(image.data, stagingBuffer.mappedData, image.faceCount * image.faceSize);

    writeImage(image, skyboxPath);
    delete[] image.data;

    vkDestroyDescriptorSetLayout(device, setLayout, nullptr);
    vkDestroyShaderModule(device, generateSkyboxShader, nullptr);
    vkDestroyPipelineLayout(device, generateSkyboxPipelineLayout, nullptr);
    vkDestroyPipeline(device, generateSkyboxPipeline, nullptr);
    destroyGpuImage(hdriGpuImage);
    destroyGpuImage(skyboxGpuImage);
}