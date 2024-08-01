#pragma once

#include "Graphics.hpp"
#ifdef DEBUG
#include "VkUtils.hpp"

void initRenderdoc();
void startRenderdocCapture();
void endRenderdocCapture();

void setGpuImageName(GpuImage &gpuImage, const char *name);

inline void beginCmdLabel(Cmd cmd, const char *name)
{
    VkDebugUtilsLabelEXT debugLabel = initDebugUtilsLabelEXT(name);
    vkCmdBeginDebugUtilsLabelEXT(cmd.commandBuffer, &debugLabel);
}

inline void endCmdLabel(Cmd cmd)
{
    vkCmdEndDebugUtilsLabelEXT(cmd.commandBuffer);
}
#else
inline void initRenderdoc() {}
inline void startRenderdocCapture() {}
inline void endRenderdocCapture() {}
inline void setGpuImageName(GpuImage &, const char *) {}
inline void beginCmdLabel(Cmd, const char *) {}
inline void endCmdLabel(Cmd) {}
#endif // DEBUG

#ifdef TRACY_ENABLE
#define TRACY_VK_USE_SYMBOL_TABLE
#include <tracy/Tracy.hpp>
#include <tracy/TracyVulkan.hpp>

#define GpuZoneCollect(cmd) TracyVkCollect(tracyContext, cmd.commandBuffer)

extern TracyVkCtx tracyContext;

class TracyCollectHelper
{
public:
    TracyCollectHelper(Cmd cmd) : cmd { cmd } {}
    ~TracyCollectHelper() { GpuZoneCollect(cmd); }
private:
    Cmd cmd;
};

#ifdef DEBUG
#define ScopedGpuZone(cmd, name) TracyVkZone(tracyContext, cmd.commandBuffer, name); ScopedGpuZoneImpl TracyConcat(__ScopedGpuZone, TracyLine)(cmd, name)

class ScopedGpuZoneImpl
{
public:
    ScopedGpuZoneImpl(Cmd cmd, const char *name) : cmd { cmd } { beginCmdLabel(cmd, name); }
    ~ScopedGpuZoneImpl() { endCmdLabel(cmd); }
private:
    Cmd cmd;
};
#else
#define ScopedGpuZone(cmd, name) TracyVkZone(tracyContext, cmd.commandBuffer, name)
#endif // DEBUG

#define ScopedGpuZoneAutoCollect(cmd, name) TracyCollectHelper TracyConcat(__TracyCollectHelper, TracyLine)(cmd); ScopedGpuZone(cmd, name)

#else
#define GpuZoneCollect(cmd)
#define ScopedGpuZone(cmd, name)
#define ScopedGpuZoneAutoCollect(cmd, name)
#endif // TRACY_ENABLE