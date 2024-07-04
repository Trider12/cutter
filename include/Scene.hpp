#pragma once

#include "../src/shaders/common.h"
#include "Glm.hpp"
#include "Graphics.hpp"
#include "ImageUtils.hpp"

#include <string>
#include <vector>

struct MaterialTextureSet
{
    const char *baseColorTexPath;
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
    std::vector<std::string> imagePaths;
};

void importMaterial(const MaterialTextureSet &srcSet, const MaterialTextureSet &dstSet);

void addMaterialToScene(Scene &scene, const MaterialTextureSet &set);

void importSceneFromGlb(const char *glbFilePath, const char *sceneDirPath, float scale);

Scene loadSceneFromFile(const char *sceneDirPath);

void writeSceneToFile(const Scene &scene, const char *sceneDirPath);