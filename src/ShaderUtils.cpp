#include "ShaderUtils.hpp"
#include "VkUtils.hpp"

#include <stdio.h>
#include <string.h>

#include <shaderc/shaderc.h>

static shaderc_compiler_t compiler;
static shaderc_compile_options_t options;

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
    options = shaderc_compile_options_initialize();
    shaderc_compile_options_set_source_language(options, shaderc_source_language_glsl);
    shaderc_compile_options_set_forced_version_profile(options, 450, shaderc_profile_none);
    shaderc_compile_options_set_include_callbacks(options, shadercIncludeResolve, shadercIncludeResultRelease, nullptr);
    shaderc_compile_options_set_target_env(options, shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
    shaderc_compile_options_set_target_spirv(options, shaderc_spirv_version_1_5);
    shaderc_compile_options_set_warnings_as_errors(options);
#ifdef DEBUG
    shaderc_compile_options_set_generate_debug_info(options);
    shaderc_compile_options_add_macro_definition(options, "DEBUG", 5, nullptr, 0);
#else
    shaderc_compile_options_set_optimization_level(options, shaderc_optimization_level_performance);
#endif // DEBUG
}

void terminateShaderCompiler()
{
    shaderc_compiler_release(compiler);
    shaderc_compile_options_release(options);
}

void compileShaderIntoSpv(const char *shaderFilename, const char *spvFilename, ShaderType type)
{
    uint32_t fileSize = readFile(shaderFilename, nullptr, 0);
    char *fileData = new char[fileSize];
    readFile(shaderFilename, (uint8_t *)fileData, fileSize);
    shaderc_compilation_result_t result = shaderc_compile_into_spv(compiler, fileData, fileSize, (shaderc_shader_kind)type, shaderFilename, "main", options);
    shaderc_compilation_status status = shaderc_result_get_compilation_status(result);

    if (status == shaderc_compilation_status_compilation_error)
    {
        fputs(shaderc_result_get_error_message(result), stderr);
    }

    ASSERT(status == shaderc_compilation_status_success);
    writeFile(spvFilename, shaderc_result_get_bytes(result), (uint32_t)shaderc_result_get_length(result));
    shaderc_result_release(result);
    delete[] fileData;
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