#pragma once

#include "Utils.hpp"

#include <stdint.h>
#include <volk/volk.h>

enum class ImagePurpose : uint8_t
{
    Undefined = 0,
    Color,
    Normal,
    Shading,
    HDRI
};

struct Image
{
    uint8_t *data;
    uint32_t dataSize;
    uint16_t width, height;
    VkFormat format : 32;
    uint8_t faceCount, levelCount;
    ImagePurpose purpose;
    uint8_t pad;
};
static_assert(sizeof(Image) == 24, "");

EnumBool(GenerateMips);
EnumBool(Compress);

void initImageUtils(const char *generateSkyboxShaderPath, const char *generateMipsShaderPath);

void terminateImageUtils();

Image importImage(const uint8_t *data, uint32_t dataSize, ImagePurpose purpose);

Image importImage(const char *inImageFilename, ImagePurpose purpose);

void importImage(const char *inImageFilename, const char *outImageFilename, ImagePurpose purpose);

void writeImage(Image &image, const char *outImageFilename, GenerateMips generateMips, Compress compress);

Image loadImage(const char *inImageFilename);

void freeImage(Image &image);

typedef void (*IterateCallback)(const Image &image, uint8_t level, uint8_t face, uint16_t mipWidth, uint16_t mipHeight, uint32_t dataOffset, uint32_t dataSize, void *userData);

uint32_t iterateImageLevelFaces(const Image &image, IterateCallback callback, void *userData);

void generateSkybox(const char *hdriPath, const char *skyboxPath);