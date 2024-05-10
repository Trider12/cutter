#include "Scene.hpp"

#include <stack>

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
#include <meshoptimizer.h>

static void calcSizesAndOffsets(Scene &scene)
{
    scene.indexesSize = (uint32_t)scene.indices.size() * sizeof(decltype(scene.indices)::value_type);
    scene.positionsSize = (uint32_t)scene.positions.size() * sizeof(decltype(scene.positions)::value_type);
    scene.normalUvsSize = (uint32_t)scene.normalUvs.size() * sizeof(decltype(scene.normalUvs)::value_type);
    scene.matricesSize = (uint32_t)scene.matrices.size() * sizeof(decltype(scene.matrices)::value_type);
    scene.indicesOffset = 0;
    scene.positionsOffset = scene.indicesOffset + scene.indexesSize;
    scene.normalUvsOffset = scene.positionsOffset + scene.positionsSize;
    scene.matricesOffset = scene.normalUvsOffset + scene.normalUvsSize;
}

Scene loadSceneFromGltf(const char *filename)
{
    ASSERT(EXISTS(filename));

    cgltf_options options {};
    cgltf_data *data = nullptr;
    VERIFY(cgltf_parse_file(&options, filename, &data) == cgltf_result_success);
    ASSERT(cgltf_validate(data) == cgltf_result_success);
    VERIFY(cgltf_load_buffers(&options, data, filename) == cgltf_result_success);

    const cgltf_scene &cgltfScene = data->scene ? *data->scene : data->scenes[0];

    std::stack<cgltf_node *> stack;

    for (cgltf_size i = 0; i < cgltfScene.nodes_count; i++)
    {
        stack.push(cgltfScene.nodes[i]);
    }

    Scene scene;
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

        glm::mat4 matrix;
        cgltf_node_transform_world(node, glm::value_ptr(matrix));
        scene.matrices.push_back(matrix);

        for (cgltf_size i = 0; i < node->mesh->primitives_count; i++)
        {
            const cgltf_primitive &primitive = node->mesh->primitives[i];
            cgltf_size indexCount = primitive.indices->count;
            cgltf_size vertexCount = primitive.attributes[0].data->count;
            size_t oldSceneIndicesSize = scene.indices.size();
            size_t oldSceneVerticesSize = scene.positions.size();
            scene.indices.resize(oldSceneIndicesSize + indexCount, (uint32_t)oldSceneIndicesSize);
            scene.positions.resize(oldSceneVerticesSize + vertexCount);
            scene.normalUvs.resize(oldSceneVerticesSize + vertexCount);

            for (cgltf_size j = 0; j < indexCount; j++)
            {
                scene.indices[oldSceneIndicesSize + j] += (uint32_t)cgltf_accessor_read_index(primitive.indices, j);
            }

            for (cgltf_size j = 0; j < primitive.attributes_count; j++)
            {
                const cgltf_attribute &attribute = primitive.attributes[j];

                switch (attribute.type)
                {
                case cgltf_attribute_type_position:
                    for (cgltf_size k = 0; k < vertexCount; k++)
                    {
                        cgltf_accessor_read_float(attribute.data, k, values, COUNTOF(values));
                        Position &position = scene.positions[oldSceneVerticesSize + k];
                        position.x = meshopt_quantizeHalf(values[0]);
                        position.y = meshopt_quantizeHalf(values[1]);
                        position.z = meshopt_quantizeHalf(values[2]);
                        position.matrixIndex = (uint16_t)scene.matrices.size() - 1;
                    }
                    break;
                case cgltf_attribute_type_normal:
                    for (cgltf_size k = 0; k < vertexCount; k++)
                    {
                        cgltf_accessor_read_float(attribute.data, k, values, COUNTOF(values));
                        NormalUv &normal = scene.normalUvs[oldSceneVerticesSize + k];
                        normal.nx = (uint8_t)meshopt_quantizeSnorm(values[0], 8);
                        normal.ny = (uint8_t)meshopt_quantizeSnorm(values[1], 8);
                        normal.nz = (uint8_t)meshopt_quantizeSnorm(values[2], 8);
                    }
                    break;
                case cgltf_attribute_type_texcoord:
                    for (cgltf_size k = 0; k < vertexCount; k++)
                    {
                        cgltf_accessor_read_float(attribute.data, k, values, COUNTOF(values));
                        NormalUv &uv = scene.normalUvs[oldSceneVerticesSize + k];
                        uv.u = (uint16_t)meshopt_quantizeUnorm(values[0], 16);
                        uv.u = (uint16_t)meshopt_quantizeUnorm(values[1], 16);
                    }
                    break;
                default:
                    break;
                }
            }
        }
    }

    ASSERT(scene.positions.size() == scene.normalUvs.size());

    calcSizesAndOffsets(scene);

    return scene;
}

Scene loadSceneFromFile(const char *filename)
{
    FILE *stream = fopen(filename, "rb");
    ASSERT(stream);

    uint32_t indexCount;
    uint32_t vertexCount;
    uint32_t matrixCount;
    fread(&indexCount, sizeof(uint32_t), 1, stream);
    fread(&vertexCount, sizeof(uint32_t), 1, stream);
    fread(&matrixCount, sizeof(uint32_t), 1, stream);

    Scene scene;
    scene.indices.resize(indexCount);
    scene.positions.resize(vertexCount);
    scene.normalUvs.resize(vertexCount);
    scene.matrices.resize(matrixCount);

    fread(scene.indices.data(), sizeof(decltype(scene.indices)::value_type), indexCount, stream);
    fread(scene.positions.data(), sizeof(decltype(scene.positions)::value_type), vertexCount, stream);
    fread(scene.normalUvs.data(), sizeof(decltype(scene.normalUvs)::value_type), vertexCount, stream);
    fread(scene.matrices.data(), sizeof(decltype(scene.matrices)::value_type), matrixCount, stream);

    calcSizesAndOffsets(scene);

    return scene;
}

void writeSceneToFile(const Scene &scene, const char *filename)
{
    FILE *stream = fopen(filename, "wb");
    ASSERT(stream);

    ASSERT(scene.positions.size() == scene.normalUvs.size());
    uint32_t indexCount = (uint32_t)scene.indices.size();
    uint32_t vertexCount = (uint32_t)scene.positions.size();
    uint32_t matrixCount = (uint32_t)scene.positions.size();

    fwrite(&indexCount, sizeof(uint32_t), 1, stream);
    fwrite(&vertexCount, sizeof(uint32_t), 1, stream);
    fwrite(&matrixCount, sizeof(uint32_t), 1, stream);
    fwrite(scene.indices.data(), sizeof(decltype(scene.indices)::value_type), indexCount, stream);
    fwrite(scene.positions.data(), sizeof(decltype(scene.positions)::value_type), vertexCount, stream);
    fwrite(scene.normalUvs.data(), sizeof(decltype(scene.normalUvs)::value_type), vertexCount, stream);
    fwrite(scene.matrices.data(), sizeof(decltype(scene.matrices)::value_type), matrixCount, stream);
}