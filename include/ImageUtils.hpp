#pragma once

#include "Utils.hpp"

#include <stdint.h>
#include <volk/volk.h>

enum ImagePurpose : uint8_t
{
    Undefined = 0,
    Color,
    Normal,
    Shading,
    HDRI
};

struct Image
{
    uint8_t *data; // dataSize = faceCount * layerSize;
    uint32_t faceSize;
    uint16_t width, height;
    VkFormat format : 32;
    uint8_t faceCount, levelCount;
    ImagePurpose purpose;
    uint8_t pad;
};

static_assert(sizeof(Image) == 24, "");

Image importImage(const uint8_t *data, uint32_t dataSize, ImagePurpose purpose);

Image importImage(const char *inImageFilename, ImagePurpose purpose);

void importImage(const char *inImageFilename, const char *outImageFilename, ImagePurpose purpose);

void writeImage(Image &image, const char *outImageFilename);

Image loadImage(const char *inImageFilename);

void freeImage(Image &image);