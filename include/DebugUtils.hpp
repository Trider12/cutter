#pragma once

void initRenderdoc();

void startRenderdocCapture();

void endRenderdocCapture();

#ifdef TRACY_ENABLE
#include <volk/volk.h>
#define TRACY_VK_USE_SYMBOL_TABLE
#include <tracy/Tracy.hpp>
#include <tracy/TracyVulkan.hpp>

extern TracyVkCtx tracyContext;

#ifdef DEBUG
#define ScopedGpuZone(cmd, name) TracyCollectHelper TracyConcat(__TracyCollectHelper, TracyLine)(cmd); TracyVkZone(tracyContext, cmd.commandBuffer, name); ScopedGpuZoneImpl TracyConcat(__ScopedGpuZone, TracyLine)(cmd, name)

#include "Graphics.hpp"
#include "VkUtils.hpp"

class TracyCollectHelper
{
public:
    TracyCollectHelper(Cmd cmd) : cmd { cmd } {}
    ~TracyCollectHelper() { TracyVkCollect(tracyContext, cmd.commandBuffer); }
private:
    Cmd cmd;
};

class ScopedGpuZoneImpl
{
public:
    ScopedGpuZoneImpl(Cmd cmd, const char *name) : cmd { cmd }
    {
        VkDebugUtilsLabelEXT debugLabel = initDebugUtilsLabelEXT(name);
        vkCmdBeginDebugUtilsLabelEXT(cmd.commandBuffer, &debugLabel);
    }
    ~ScopedGpuZoneImpl()
    {
        vkCmdEndDebugUtilsLabelEXT(cmd.commandBuffer);
    }
private:
    Cmd cmd;
};
#else
#define ScopedGpuZone(cmd, name) TracyVkZone(tracyContext, cmd.commandBuffer, name)
#endif // DEBUG

#else
#define ScopedGpuZone(cmd, name)
#define ScopedGpuZoneColored(cmd, name, color)
#endif // TRACY_ENABLE