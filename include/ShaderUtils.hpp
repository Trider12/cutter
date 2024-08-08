#pragma once

#include <volk/volk.h>

enum class ShaderType : uint8_t
{
    Vertex = 0,
    Fragment = 1,
    Compute = 2
};

void initShaderCompiler();

void terminateShaderCompiler();

void compileShaderIntoSpv(const char *shaderFilename, const char *spvFilename, ShaderType type);

VkShaderModule createShaderModuleFromSpv(VkDevice device, const char *spvFilename);