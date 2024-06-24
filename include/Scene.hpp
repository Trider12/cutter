#pragma once

#include "../assets/shaders/common.h"
#include "Glm.hpp"
#include "Graphics.hpp"

#include <vector>

struct Image
{
    std::vector<char> data; // unpacked
    uint32_t width = 0, height = 0;
    bool sRGB = false;
};

struct MaterialTextureSet
{
    const char *colorTexPath;
    const char *normalTexPath;
    const char *aoRoughMetalTexPath;
};

struct Scene
{
    std::vector<uint32_t> indices;
    std::vector<Position> positions;
    std::vector<NormalUv> normalUvs;
    std::vector<TransformData> transforms;
    std::vector<MaterialData> materials;
    std::vector<Image> images;
};

void addMaterialToScene(Scene &scene, const MaterialTextureSet &set);

Scene importSceneFromGlb(const char *filename, float scale);

Scene loadSceneFromFile(const char *filename);

void writeSceneToFile(const Scene &scene, const char *filename);