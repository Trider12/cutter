#include "Scene.hpp"
#include "ImageUtils.hpp"
#include "VkUtils.hpp"

#include <stack>

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
#include <cwalk.h>
#include <meshoptimizer.h>

static const char *const sceneFileExtension = ".bin";
static const char *const textureFileExtension = ".ktx2";

static void importImage(const cgltf_image *image, const char *path, ImagePurpose purpose)
{
    uint8_t *data = (uint8_t *)image->buffer_view->buffer->data + image->buffer_view->offset;
    uint32_t dataSize = (uint32_t)image->buffer_view->size;
    Image img = importImage(data, dataSize, purpose);
    writeImage(img, path);
    freeImage(img);
}

static void importMaterials(const cgltf_data *data, Scene &scene, const char *sceneDirPath)
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

    char imagePath[256];
    uint32_t imagePathSize = snprintf(imagePath, sizeof(imagePath), "%s/", sceneDirPath);
    scene.materials.reserve(data->materials_count);
    scene.imagePaths.reserve(data->materials_count * 3);

    for (cgltf_size i = 0; i < data->materials_count; i++)
    {
        MaterialData md;
        md.colorTexIndex = (uint32_t)(i * 3 + 0);
        snprintf(imagePath + imagePathSize, sizeof(imagePath) - imagePathSize, "image%02u%s", md.colorTexIndex, textureFileExtension);
        importImage(data->materials[i].pbr_metallic_roughness.base_color_texture.texture->image, imagePath, ImagePurpose::Color);
        scene.imagePaths.push_back(imagePath);

        md.normalTexIndex = (uint32_t)(i * 3 + 1);
        snprintf(imagePath + imagePathSize, sizeof(imagePath) - imagePathSize, "image%02u%s", md.normalTexIndex, textureFileExtension);
        importImage(data->materials[i].normal_texture.texture->image, imagePath, ImagePurpose::Normal);
        scene.imagePaths.push_back(imagePath);

        md.aoRoughMetalTexIndex = (uint32_t)(i * 3 + 2);
        snprintf(imagePath + imagePathSize, sizeof(imagePath) - imagePathSize, "image%02u%s", md.aoRoughMetalTexIndex, textureFileExtension);
        importImage(data->materials[i].pbr_metallic_roughness.metallic_roughness_texture.texture->image, imagePath, ImagePurpose::Shading);
        scene.imagePaths.push_back(imagePath);

        scene.materials.push_back(md);
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

void importMaterial(const MaterialTextureSet &srcSet, const MaterialTextureSet &dstSet)
{
    importImage(srcSet.baseColorTexPath, dstSet.baseColorTexPath, ImagePurpose::Color);
    importImage(srcSet.normalTexPath, dstSet.normalTexPath, ImagePurpose::Normal);
    importImage(srcSet.aoRoughMetalTexPath, dstSet.aoRoughMetalTexPath, ImagePurpose::Shading);
}

void addMaterialToScene(Scene &scene, const MaterialTextureSet &set)
{
    uint32_t imageCount = (uint32_t)scene.imagePaths.size();
    MaterialData md;
    md.colorTexIndex = imageCount;
    md.normalTexIndex = imageCount + 1;
    md.aoRoughMetalTexIndex = imageCount + 2;
    scene.materials.push_back(md);

    scene.imagePaths.push_back(set.baseColorTexPath);
    scene.imagePaths.push_back(set.normalTexPath);
    scene.imagePaths.push_back(set.aoRoughMetalTexPath);
}

void importSceneFromGlb(const char *glbFilePath, const char *sceneDirPath, float scale)
{
    ASSERT(pathExists(glbFilePath));
    mkdir(sceneDirPath, true);

    cgltf_options options {};
    cgltf_data *data = nullptr;
    VERIFY(cgltf_parse_file(&options, glbFilePath, &data) == cgltf_result_success);
    ASSERT(cgltf_validate(data) == cgltf_result_success);
    VERIFY(cgltf_load_buffers(&options, data, glbFilePath) == cgltf_result_success);

    Scene scene;
    importMaterials(data, scene, sceneDirPath);

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

    cgltf_free(data);

    ASSERT(scene.positions.size() == scene.normalUvs.size());
    optimizeScene(scene);

    writeSceneToFile(scene, sceneDirPath);
}

static const char *getSceneFilename(const char *sceneDirPath)
{
    const char *sceneDirBasename;
    size_t sceneDirBasenameLength;
    cwk_path_get_basename_wout_extension(sceneDirPath, &sceneDirBasename, &sceneDirBasenameLength);
    const char *format = "%s/%.*s%s";
    size_t sceneFilenameSize = snprintf(nullptr, 0, format, sceneDirPath, (uint32_t)sceneDirBasenameLength, sceneDirBasename, sceneFileExtension) + 1;
    char *sceneFilename = new char[sceneFilenameSize];
    snprintf(sceneFilename, sceneFilenameSize, format, sceneDirPath, (uint32_t)sceneDirBasenameLength, sceneDirBasename, sceneFileExtension);
    return sceneFilename;
}

Scene loadSceneFromFile(const char *sceneDirPath)
{
    const char *sceneFilename = getSceneFilename(sceneDirPath);
    FILE *stream = fopen(sceneFilename, "rb");
    ASSERT(stream);
    delete[] sceneFilename;

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
    scene.imagePaths.resize(imageCount);

    fread(scene.indices.data(), sizeof(decltype(scene.indices)::value_type), indexCount, stream);
    fread(scene.positions.data(), sizeof(decltype(scene.positions)::value_type), vertexCount, stream);
    fread(scene.normalUvs.data(), sizeof(decltype(scene.normalUvs)::value_type), vertexCount, stream);
    fread(scene.transforms.data(), sizeof(decltype(scene.transforms)::value_type), transformCount, stream);
    fread(scene.materials.data(), sizeof(decltype(scene.materials)::value_type), materialCount, stream);

    for (uint32_t i = 0; i < imageCount; i++)
    {
        uint32_t size;
        fread(&size, sizeof(uint32_t), 1, stream);
        scene.imagePaths[i].resize(size);
        fread((void *)scene.imagePaths[i].data(), 1, size, stream);
    }

    fclose(stream);

    return scene;
}

void writeSceneToFile(const Scene &scene, const char *sceneDirPath)
{
    mkdir(sceneDirPath, true);
    const char *sceneFilename = getSceneFilename(sceneDirPath);
    FILE *stream = fopen(sceneFilename, "wb");
    ASSERT(stream);
    delete[] sceneFilename;

    ASSERT(scene.positions.size() == scene.normalUvs.size());
    uint32_t indexCount = (uint32_t)scene.indices.size();
    uint32_t vertexCount = (uint32_t)scene.positions.size();
    uint32_t transformCount = (uint32_t)scene.transforms.size();
    uint32_t materialCount = (uint32_t)scene.materials.size();
    uint32_t imageCount = (uint32_t)scene.imagePaths.size();

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

    for (uint32_t i = 0; i < imageCount; i++)
    {
        uint32_t size = (uint32_t)scene.imagePaths[i].size();
        fwrite(&size, sizeof(uint32_t), 1, stream);
        fwrite(scene.imagePaths[i].data(), 1, size, stream);
    }

    fclose(stream);
}