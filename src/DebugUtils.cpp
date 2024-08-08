#ifdef DEBUG
#include "DebugUtils.hpp"
#include "Utils.hpp"

#include <renderdoc_app.h>

extern VkDevice device;

static RENDERDOC_API_1_6_0 *renderdocApi = nullptr;
#ifdef _WIN32
#include <Windows.h>

void initRenderdoc()
{
    if (HMODULE mod = GetModuleHandleA("renderdoc.dll"))
    {
        pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
        int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_6_0, (void **)&renderdocApi);
        ASSERT(ret == 1);
    }
}
#else // assume Linux
#include <dlfcn.h>

void initRenderdoc()
{
    if (void *mod = dlopen("librenderdoc.so", RTLD_NOW | RTLD_NOLOAD))
    {
        pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)dlsym(mod, "RENDERDOC_GetAPI");
        int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_6_0, (void **)&renderdocApi);
        ASSERT(ret == 1);
    }
}
#endif // _WIN32

void startRenderdocCapture()
{
    if (renderdocApi)
        renderdocApi->StartFrameCapture(nullptr, nullptr);
}

void endRenderdocCapture()
{
    if (renderdocApi)
        renderdocApi->EndFrameCapture(nullptr, nullptr);
}

void setGpuBufferName(GpuBuffer &gpuBuffer, const char *name)
{
    VkDebugUtilsObjectNameInfoEXT objectNameInfo = initDebugUtilsObjectNameInfoEXT(VK_OBJECT_TYPE_BUFFER, gpuBuffer.buffer, name);
    vkSetDebugUtilsObjectNameEXT(device, &objectNameInfo);
}

void setGpuImageName(GpuImage &gpuImage, const char *name)
{
    VkDebugUtilsObjectNameInfoEXT objectNameInfo = initDebugUtilsObjectNameInfoEXT(VK_OBJECT_TYPE_IMAGE, gpuImage.image, name);
    vkSetDebugUtilsObjectNameEXT(device, &objectNameInfo);
}

#endif // DEBUG