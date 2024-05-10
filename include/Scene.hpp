#pragma once

#include "../assets/shaders/common.h"
#include "VKUtils.hpp"

#include <vector> // TODO: remove this from the header somehow

#include <Glm.hpp>

struct Scene
{
    std::vector<uint32_t> indices;
    std::vector<Position> positions;
    std::vector<NormalUv> normalUvs;
    std::vector<glm::mat4> matrices;

    uint32_t indexesSize;
    uint32_t positionsSize;
    uint32_t normalUvsSize;
    uint32_t matricesSize;
    uint32_t indicesOffset;
    uint32_t positionsOffset;
    uint32_t normalUvsOffset;
    uint32_t matricesOffset;

    AllocatedBuffer buffer;
};

Scene loadSceneFromGltf(const char *filename);

Scene loadSceneFromFile(const char *filename);

void writeSceneToFile(const Scene &scene, const char *filename);