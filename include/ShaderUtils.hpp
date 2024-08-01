#pragma once

#include <volk/volk.h>

enum ShaderType : uint8_t
{
    Vertex,
    Fragment,
    Compute
};

void initShaderCompiler();

void terminateShaderCompiler();

void compileShaderIntoSpv(const char *shaderFilename, const char *spvFilename, ShaderType type);

VkShaderModule createShaderModuleFromSpv(VkDevice device, const char *spvFilename);