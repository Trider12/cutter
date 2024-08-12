#include "ShaderUtils.hpp"
#include "VkUtils.hpp"

#include <stdio.h>
#include <string.h>

#include <shaderc/shaderc.h>

static shaderc_compiler_t compiler;
static shaderc_compile_options_t options[3];

static shaderc_include_result *shadercIncludeResolve(void *userData,
    const char *requestedSource,
    int type,
    const char *requestingSource,
    size_t includeDepth)
{
    UNUSED(userData);
    UNUSED(type);
    UNUSED(includeDepth);

    // caching might be a good idea
    const char *format = "%s/../%s";
    int stringSize = snprintf(nullptr, 0, format, requestingSource, requestedSource) + 1;
    char *filepath = new char[stringSize];
    snprintf(filepath, stringSize, format, requestingSource, requestedSource);
    uint32_t fileSize = readFile(filepath, nullptr, 0);
    char *fileData = new char[fileSize];
    readFile(filepath, (uint8_t *)fileData, fileSize);

    shaderc_include_result *result = new shaderc_include_result();
    result->source_name = filepath;
    result->source_name_length = stringSize;
    result->content = fileData;
    result->content_length = fileSize;

    return result;
}

static void shadercIncludeResultRelease(void *userData, shaderc_include_result *result)
{
    UNUSED(userData);

    delete[] result->source_name;
    delete[] result->content;
    delete result;
}

void initShaderCompiler()
{
    compiler = shaderc_compiler_initialize();
    options[0] = shaderc_compile_options_initialize();
    shaderc_compile_options_set_source_language(options[0], shaderc_source_language_glsl);
    shaderc_compile_options_set_forced_version_profile(options[0], 450, shaderc_profile_none);
    shaderc_compile_options_set_include_callbacks(options[0], shadercIncludeResolve, shadercIncludeResultRelease, nullptr);
    shaderc_compile_options_set_target_env(options[0], shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
    shaderc_compile_options_set_target_spirv(options[0], shaderc_spirv_version_1_5);
    shaderc_compile_options_set_warnings_as_errors(options[0]);
#ifdef DEBUG
    shaderc_compile_options_set_generate_debug_info(options[0]);
    shaderc_compile_options_add_macro_definition(options[0], "DEBUG", 5, nullptr, 0);
#else
    shaderc_compile_options_set_optimization_level(options[0], shaderc_optimization_level_performance);
#endif // DEBUG

    options[1] = shaderc_compile_options_clone(options[0]);
    options[2] = shaderc_compile_options_clone(options[0]);

    shaderc_compile_options_add_macro_definition(options[0], "VERTEX", 6, nullptr, 0);
    shaderc_compile_options_add_macro_definition(options[1], "FRAGMENT", 8, nullptr, 0);
    shaderc_compile_options_add_macro_definition(options[2], "COMPUTE", 7, nullptr, 0);
}

void terminateShaderCompiler()
{
    shaderc_compiler_release(compiler);
    shaderc_compile_options_release(options[0]);
    shaderc_compile_options_release(options[1]);
    shaderc_compile_options_release(options[2]);
}

bool compileShaderIntoSpv(const char *shaderFilename, const char *spvFilename, ShaderType type)
{
    ASSERT(compiler);
    uint32_t fileSize = readFile(shaderFilename, nullptr, 0);
    char *fileData = new char[fileSize];
    readFile(shaderFilename, (uint8_t *)fileData, fileSize);

    shaderc_compilation_result_t result = shaderc_compile_into_spv(compiler, fileData, fileSize, (shaderc_shader_kind)type, shaderFilename, "main", options[(uint8_t)type]);
    shaderc_compilation_status status = shaderc_result_get_compilation_status(result);

    if (status)
        fputs(shaderc_result_get_error_message(result), stderr);
    else
        writeFile(spvFilename, shaderc_result_get_bytes(result), (uint32_t)shaderc_result_get_length(result));

    shaderc_result_release(result);
    delete[] fileData;

    return !status;
}

VkShaderModule createShaderModuleFromSpv(VkDevice device, const char *spvFilename)
{
    uint32_t fileSize = readFile(spvFilename, nullptr, 0);
    uint8_t *fileData = new uint8_t[fileSize];
    readFile(spvFilename, fileData, fileSize);

    VkShaderModule shaderModule;
    VkShaderModuleCreateInfo createInfo {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = fileSize;
    createInfo.pCode = (uint32_t *)fileData;
    vkVerify(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule));
    delete[] fileData;

    return shaderModule;
}