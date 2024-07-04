#include "ImageUtils.hpp"
#include "JobSystem.hpp"
#include "Utils.hpp"

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

Image importImage(const uint8_t *data, uint32_t dataSize, ImagePurpose purpose)
{
    ZoneScoped;
    ASSERT(data);
    ASSERT(dataSize);
    ASSERT(purpose);
    Image image {};
    int w = 0, h = 0;

    switch (purpose)
    {
    case Color:
        image.data = stbi_load_from_memory(data, dataSize, &w, &h, nullptr, STBI_rgb_alpha);
        image.faceSize = w * h * STBI_rgb_alpha;
        image.format = VK_FORMAT_R8G8B8A8_SRGB;
        break;
    case Normal:
        image.data = stbi_load_from_memory(data, dataSize, &w, &h, nullptr, STBI_rgb);
        image.faceSize = w * h * STBI_rgb;
        image.format = VK_FORMAT_R8G8B8_UNORM;
        break;
    case Shading:
        image.data = stbi_load_from_memory(data, dataSize, &w, &h, nullptr, STBI_rgb_alpha);
        image.faceSize = w * h * STBI_rgb_alpha;
        image.format = VK_FORMAT_R8G8B8A8_UNORM;
        break;
    case HDRI:
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
    void *options;
};

static constexpr float defaultCompressionQuality = 0.1f;
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
        CompressBlockBC7(srcBlock, stride, dstBlock, opt.options);
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

        CompressBlockBC5(red, 4, green, 4, dstBlock, opt.options);
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

        CompressBlockBC6(rgb[0], 4 * 3, dstBlock, opt.options);
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
    const uint16_t blockCountX = image.width / srcBlockSizeInTexels, blockCountY = image.height / srcBlockSizeInTexels;
    const uint32_t dstDataFaceSize = blockCountX * blockCountY * dstBlockSizeInBytes;
    uint8_t *dstData = nullptr;
    VkFormat dstFormat = VK_FORMAT_UNDEFINED;

    switch (image.purpose)
    {
    case Color:
    case Shading:
    {
        if (image.format == VK_FORMAT_BC7_SRGB_BLOCK || image.format == VK_FORMAT_BC7_UNORM_BLOCK)
            break;

        ASSERT(image.format == VK_FORMAT_R8G8B8A8_SRGB || image.format == VK_FORMAT_R8G8B8A8_UNORM);
        dstData = new uint8_t[image.faceCount * dstDataFaceSize];
        dstFormat = image.purpose == Color ? VK_FORMAT_BC7_SRGB_BLOCK : image.purpose == Shading ? VK_FORMAT_BC7_UNORM_BLOCK : VK_FORMAT_UNDEFINED;
        ASSERT(dstFormat);

        CompressBlockRowOptions opt;
        opt.blockCountX = blockCountX;
        opt.imageWidth = image.width;
        opt.channelCount = 4;
        opt.src = image.data;
        opt.dst = dstData;
        CreateOptionsBC7(&opt.options);
        SetQualityBC7(opt.options, defaultCompressionQuality);
        Token token = createToken();

        for (uint8_t i = 0; i < image.faceCount; i++, opt.src += image.faceSize, opt.dst += dstDataFaceSize)
        {
            for (uint32_t y = 0; y < blockCountY; y++)
            {
                enqueueJob(compressBlockRowBC7, y, &opt, token);
            }
            waitForToken(token);
        }

        DestroyOptionsBC7(opt.options);
        destroyToken(token);
        break;
    }
    case Normal:
    {
        if (image.format == VK_FORMAT_BC5_UNORM_BLOCK)
            break;

        ASSERT(image.format == VK_FORMAT_R8G8B8_UNORM || image.format == VK_FORMAT_R8G8B8A8_UNORM);
        dstData = new uint8_t[image.faceCount * dstDataFaceSize];
        dstFormat = VK_FORMAT_BC5_UNORM_BLOCK;

        CompressBlockRowOptions opt;
        opt.blockCountX = blockCountX;
        opt.imageWidth = image.width;
        opt.channelCount = image.format == VK_FORMAT_R8G8B8_UNORM ? 3 : image.format == VK_FORMAT_R8G8B8A8_UNORM ? 4 : 0;
        opt.src = image.data;
        opt.dst = dstData;
        ASSERT(opt.channelCount);
        CreateOptionsBC5(&opt.options);
        SetQualityBC5(opt.options, defaultCompressionQuality);
        Token token = createToken();

        for (uint8_t i = 0; i < image.faceCount; i++, opt.src += image.faceSize, opt.dst += dstDataFaceSize)
        {
            for (uint32_t y = 0; y < blockCountY; y++)
            {
                enqueueJob(compressBlockRowBC5CopyChannels, y, &opt, token);
            }
            waitForToken(token);
        }

        DestroyOptionsBC5(opt.options);
        destroyToken(token);
        break;
    }
    case HDRI:
    {
        if (image.format == VK_FORMAT_BC6H_UFLOAT_BLOCK)
            break;

        ASSERT(image.format == VK_FORMAT_R16G16B16_SFLOAT || image.format == VK_FORMAT_R16G16B16A16_SFLOAT);
        dstData = new uint8_t[image.faceCount * dstDataFaceSize];
        dstFormat = VK_FORMAT_BC6H_UFLOAT_BLOCK;

        CompressBlockRowOptions opt;
        opt.blockCountX = blockCountX;
        opt.imageWidth = image.width;
        opt.channelCount = image.format == VK_FORMAT_R16G16B16_SFLOAT ? 3 : image.format == VK_FORMAT_R16G16B16A16_SFLOAT ? 4 : 0;
        opt.src = image.data;
        opt.dst = dstData;
        ASSERT(opt.channelCount);
        CreateOptionsBC6(&opt.options);
        SetQualityBC6(opt.options, defaultCompressionQuality);
        Token token = createToken();

        for (uint8_t i = 0; i < image.faceCount; i++, opt.src += image.faceSize, opt.dst += dstDataFaceSize)
        {
            for (uint32_t y = 0; y < blockCountY; y++)
            {
                enqueueJob(compressBlockRowBC6CopyChannels, y, &opt, token);
            }
            waitForToken(token);
        }

        DestroyOptionsBC6(opt.options);
        destroyToken(token);
        break;
    }
    default:
        ASSERT(false);
        return;
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