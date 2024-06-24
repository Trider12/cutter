#include "Scene.hpp"
#include "Stb.h"
#include "VkUtils.hpp"

#include <stack>

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
#include <meshoptimizer.h>

static Image importImage(const char *path)
{
    ASSERT(EXISTS(path));
    int x, y;
    char *imageData = (char *)stbi_load(path, &x, &y, nullptr, STBI_rgb_alpha);
    uint32_t imageDataSize = x * y * STBI_rgb_alpha;

    Image img;
    img.width = (uint32_t)x;
    img.height = (uint32_t)y;
    img.data.assign(imageData, imageData + imageDataSize);
    stbi_image_free(imageData);

    return img;
}

static Image importImage(const cgltf_image *image)
{
    int x, y;
    unsigned char *data = (unsigned char *)image->buffer_view->buffer->data + image->buffer_view->offset;
    uint32_t dataSize = (uint32_t)image->buffer_view->size;
    char *imageData = (char *)stbi_load_from_memory(data, dataSize, &x, &y, nullptr, STBI_rgb_alpha);
    uint32_t imageDataSize = x * y * STBI_rgb_alpha;

    Image img;
    img.width = (uint32_t)x;
    img.height = (uint32_t)y;
    img.data.assign(imageData, imageData + imageDataSize);
    stbi_image_free(imageData);

    return img;
}

static void importImages(const cgltf_data *data, Scene &scene)
{
    scene.images.reserve(data->images_count);

    for (cgltf_size i = 0; i < data->images_count; i++)
    {
        scene.images.push_back(importImage(data->images + i));
    }
}

static void importMaterials(const cgltf_data *data, Scene &scene)
{
    ASSERT(data->materials_count == 0 || data->materials_count == 1);

    if (data->materials_count == 0)
    {
        MaterialData md;
        md.colorTexIndex = UINT32_MAX;
        md.normalTexIndex = UINT32_MAX;
        md.aoRoughMetalTexIndex = UINT32_MAX;
        scene.materials.push_back(md);
        return;
    }

    scene.materials.reserve(data->materials_count);

    for (cgltf_size i = 0; i < data->materials_count; i++)
    {
        MaterialData md;
        md.colorTexIndex = (uint32_t)cgltf_image_index(data, data->materials[i].pbr_metallic_roughness.base_color_texture.texture->image);
        md.normalTexIndex = (uint32_t)cgltf_image_index(data, data->materials[i].normal_texture.texture->image);
        md.aoRoughMetalTexIndex = (uint32_t)cgltf_image_index(data, data->materials[i].pbr_metallic_roughness.metallic_roughness_texture.texture->image);
        scene.materials.push_back(md);
        scene.images[md.colorTexIndex].sRGB = true;
    }
}

static void optimizeScene(Scene &scene)
{
    meshopt_Stream streams[]
    {
        {scene.positions.data(), sizeof(Position), sizeof(Position)},
        {scene.normalUvs.data(), sizeof(NormalUv), sizeof(NormalUv)}
    };
    std::vector<unsigned int> remap(scene.indices.size());
    size_t newVertexCount = meshopt_generateVertexRemapMulti(remap.data(), scene.indices.data(), scene.indices.size(), scene.positions.size(), streams, COUNTOF(streams));

    std::vector<uint32_t> newIndexBuffer(scene.indices.size());
    std::vector<Position> newPositionBuffer(newVertexCount);
    std::vector<NormalUv> newNormalUvBuffer(newVertexCount);
    meshopt_remapIndexBuffer(newIndexBuffer.data(), scene.indices.data(), scene.indices.size(), remap.data());
    meshopt_remapVertexBuffer(newPositionBuffer.data(), scene.positions.data(), scene.positions.size(), sizeof(Position), remap.data());
    meshopt_remapVertexBuffer(newNormalUvBuffer.data(), scene.normalUvs.data(), scene.normalUvs.size(), sizeof(NormalUv), remap.data());

    scene.indices = newIndexBuffer;
    scene.positions = newPositionBuffer;
    scene.normalUvs = newNormalUvBuffer;
}

void addMaterialToScene(Scene &scene, const MaterialTextureSet &set)
{
    uint32_t imageCount = (uint32_t)scene.images.size();
    MaterialData md;
    md.colorTexIndex = imageCount;
    md.normalTexIndex = imageCount + 1;
    md.aoRoughMetalTexIndex = imageCount + 2;
    scene.materials.push_back(md);

    scene.images.push_back(importImage(set.colorTexPath));
    scene.images.push_back(importImage(set.normalTexPath));
    scene.images.push_back(importImage(set.aoRoughMetalTexPath));
    scene.images[imageCount].sRGB = true;
}

Scene importSceneFromGlb(const char *filename, float scale)
{
    ASSERT(EXISTS(filename));

    cgltf_options options {};
    cgltf_data *data = nullptr;
    VERIFY(cgltf_parse_file(&options, filename, &data) == cgltf_result_success);
    ASSERT(cgltf_validate(data) == cgltf_result_success);
    VERIFY(cgltf_load_buffers(&options, data, filename) == cgltf_result_success);

    Scene scene;
    importImages(data, scene);
    importMaterials(data, scene);

    const cgltf_scene &cgltfScene = data->scene ? *data->scene : data->scenes[0];
    std::stack<cgltf_node *> stack;

    for (cgltf_size i = 0; i < cgltfScene.nodes_count; i++)
    {
        stack.push(cgltfScene.nodes[i]);
    }

    float values[4];

    while (!stack.empty())
    {
        cgltf_node *node = stack.top();
        stack.pop();

        for (cgltf_size i = 0; i < node->children_count; i++)
        {
            stack.push(node->children[i]);
        }

        if (!node->mesh)
            continue;

        uint16_t transformIndex = (uint16_t)scene.transforms.size();
        TransformData td;
        cgltf_node_transform_world(node, glm::value_ptr(td.toWorldMat));
        td.toWorldMat *= scale;
        td.toWorldMat[3][3] /= scale;
        td.toLocalMat = glm::inverse(td.toWorldMat);
        scene.transforms.push_back(td);

        cgltf_material *material = nullptr;

        for (cgltf_size i = 0; i < node->mesh->primitives_count; i++)
        {
            if (node->mesh->primitives[i].material)
            {
                material = material = node->mesh->primitives[i].material;
                break;
            }
        }

        for (cgltf_size i = 0; i < node->mesh->primitives_count; i++)
        {
            const cgltf_primitive &primitive = node->mesh->primitives[i];
            cgltf_size indexCount = primitive.indices->count;
            cgltf_size vertexCount = primitive.attributes[0].data->count;
            size_t oldIndexCount = scene.indices.size();
            size_t oldVertexCount = scene.positions.size();
            scene.indices.resize(oldIndexCount + indexCount);
            scene.positions.resize(oldVertexCount + vertexCount);
            scene.normalUvs.resize(oldVertexCount + vertexCount);

            for (cgltf_size j = 0; j < indexCount; j++)
            {
                scene.indices[oldIndexCount + j] = (uint32_t)(oldVertexCount + cgltf_accessor_read_index(primitive.indices, j));
            }

            for (cgltf_size j = 0; j < primitive.attributes_count; j++)
            {
                const cgltf_attribute &attribute = primitive.attributes[j];

                switch (attribute.type)
                {
                case cgltf_attribute_type_position:
                    for (cgltf_size k = 0; k < vertexCount; k++)
                    {
                        VERIFY(cgltf_accessor_read_float(attribute.data, k, values, COUNTOF(values)));
                        Position &position = scene.positions[oldVertexCount + k];
                        position.x = meshopt_quantizeHalf(values[0]);
                        position.y = meshopt_quantizeHalf(values[1]);
                        position.z = meshopt_quantizeHalf(values[2]);
                        position.transformIndex = transformIndex;
                    }
                    break;
                case cgltf_attribute_type_normal:
                    for (cgltf_size k = 0; k < vertexCount; k++)
                    {
                        VERIFY(cgltf_accessor_read_float(attribute.data, k, values, COUNTOF(values)));
                        NormalUv &normal = scene.normalUvs[oldVertexCount + k];
                        normal.x = (int8_t)meshopt_quantizeSnorm(values[0], 8);
                        normal.y = (int8_t)meshopt_quantizeSnorm(values[1], 8);
                        normal.z = (int8_t)meshopt_quantizeSnorm(values[2], 8);
                    }
                    break;
                case cgltf_attribute_type_texcoord:
                    for (cgltf_size k = 0; k < vertexCount; k++)
                    {
                        VERIFY(cgltf_accessor_read_float(attribute.data, k, values, COUNTOF(values)));
                        NormalUv &uv = scene.normalUvs[oldVertexCount + k];
                        uv.u = (uint16_t)meshopt_quantizeHalf(values[0]);
                        uv.v = (uint16_t)meshopt_quantizeHalf(values[1]);
                    }
                    break;
                default:
                    break;
                }
            }
        }
    }

    ASSERT(scene.positions.size() == scene.normalUvs.size());
    optimizeScene(scene);

    return scene;
}

Scene loadSceneFromFile(const char *filename)
{
    FILE *stream = fopen(filename, "rb");
    ASSERT(stream);

    uint32_t indexCount;
    uint32_t vertexCount;
    uint32_t transformCount;
    uint32_t materialCount;
    uint32_t imageCount;
    fread(&indexCount, sizeof(uint32_t), 1, stream);
    fread(&vertexCount, sizeof(uint32_t), 1, stream);
    fread(&transformCount, sizeof(uint32_t), 1, stream);
    fread(&materialCount, sizeof(uint32_t), 1, stream);
    fread(&imageCount, sizeof(uint32_t), 1, stream);

    Scene scene;
    scene.indices.resize(indexCount);
    scene.positions.resize(vertexCount);
    scene.normalUvs.resize(vertexCount);
    scene.transforms.resize(transformCount);
    scene.materials.resize(materialCount);
    scene.images.resize(imageCount);

    fread(scene.indices.data(), sizeof(decltype(scene.indices)::value_type), indexCount, stream);
    fread(scene.positions.data(), sizeof(decltype(scene.positions)::value_type), vertexCount, stream);
    fread(scene.normalUvs.data(), sizeof(decltype(scene.normalUvs)::value_type), vertexCount, stream);
    fread(scene.transforms.data(), sizeof(decltype(scene.transforms)::value_type), transformCount, stream);
    fread(scene.materials.data(), sizeof(decltype(scene.materials)::value_type), materialCount, stream);

    for (uint32_t i = 0; i < imageCount; i++)
    {
        Image &image = scene.images[i];
        uint8_t temp;
        uint32_t size;
        fread(&image.width, sizeof(uint32_t), 1, stream);
        fread(&image.height, sizeof(uint32_t), 1, stream);
        fread(&temp, sizeof(uint8_t), 1, stream);
        image.sRGB = temp;
        fread(&size, sizeof(uint32_t), 1, stream);
        image.data.resize(size);
        fread(image.data.data(), 1, size, stream);
    }

    fclose(stream);

    return scene;
}

void writeSceneToFile(const Scene &scene, const char *filename)
{
    FILE *stream = fopen(filename, "wb");
    ASSERT(stream);

    ASSERT(scene.positions.size() == scene.normalUvs.size());
    uint32_t indexCount = (uint32_t)scene.indices.size();
    uint32_t vertexCount = (uint32_t)scene.positions.size();
    uint32_t transformCount = (uint32_t)scene.transforms.size();
    uint32_t materialCount = (uint32_t)scene.materials.size();
    uint32_t imageCount = (uint32_t)scene.images.size();

    // TODO: write compressed vertex data?
    fwrite(&indexCount, sizeof(uint32_t), 1, stream);
    fwrite(&vertexCount, sizeof(uint32_t), 1, stream);
    fwrite(&transformCount, sizeof(uint32_t), 1, stream);
    fwrite(&materialCount, sizeof(uint32_t), 1, stream);
    fwrite(&imageCount, sizeof(uint32_t), 1, stream);
    fwrite(scene.indices.data(), sizeof(decltype(scene.indices)::value_type), indexCount, stream);
    fwrite(scene.positions.data(), sizeof(decltype(scene.positions)::value_type), vertexCount, stream);
    fwrite(scene.normalUvs.data(), sizeof(decltype(scene.normalUvs)::value_type), vertexCount, stream);
    fwrite(scene.transforms.data(), sizeof(decltype(scene.transforms)::value_type), transformCount, stream);
    fwrite(scene.materials.data(), sizeof(decltype(scene.materials)::value_type), materialCount, stream);

    // TODO: write PNG instead of raw image data?
    for (uint32_t i = 0; i < imageCount; i++)
    {
        uint32_t size = (uint32_t)scene.images[i].data.size();
        fwrite(&scene.images[i].width, sizeof(uint32_t), 1, stream);
        fwrite(&scene.images[i].height, sizeof(uint32_t), 1, stream);
        fwrite(&scene.images[i].sRGB, sizeof(uint8_t), 1, stream);
        fwrite(&size, sizeof(uint32_t), 1, stream);
        fwrite(scene.images[i].data.data(), 1, size, stream);
    }

    fclose(stream);
}