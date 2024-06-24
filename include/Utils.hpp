#pragma once

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined _DEBUG || !defined NDEBUG
#define DEBUG
#endif

#define ASSERT assert

#ifdef DEBUG
#define VERIFY ASSERT
#else
#define VERIFY(expr) (void)(expr)
#endif

#define UNUSED(var) (void)(var)

#ifdef _MSC_VER
#define COUNTOF _countof
#include <io.h> // for access
#include <direct.h> // for mkdir
#include <malloc.h> // for alloca
#define ACCESS _access
#define MKDIR _mkdir
#define ALLOCA _alloca
#else // assume UNIX
#define COUNTOF(arr) sizeof(arr) / sizeof((arr)[0])
#include <unistd.h> // for access
#include <sys/stat.h> // for mkdir
#include <alloca.h> // for alloca
#define ACCESS access
#define MKDIR(path) mkdir(path, 0777)
#define ALLOCA alloca
#endif

#define EXISTS(path) !ACCESS(path, 0)

inline uint32_t max(uint32_t a, uint32_t b)
{
    return a > b ? a : b;
}

inline uint32_t min(uint32_t a, uint32_t b)
{
    return a < b ? a : b;
}

inline uint32_t aligned(uint32_t value, uint32_t alignment) // alignment must be a power of 2!
{
    return (value + alignment - 1) & ~(alignment - 1);
}

inline uint32_t readFile(const char *filename, char *buffer, uint32_t bufferSize)
{
    ASSERT(EXISTS(filename));
    FILE *stream = fopen(filename, "rb");
    ASSERT(stream);
    fseek(stream, 0, SEEK_END);
    uint32_t size = ftell(stream);

    if (buffer)
    {
        VERIFY(bufferSize >= size);
        fseek(stream, 0, SEEK_SET);
        size = (uint32_t)fread(buffer, 1, size, stream);
        fclose(stream);
    }

    return size;
}

inline uint32_t writeFile(const char *filename, const void *data, uint32_t size)
{
    ASSERT(data);
    FILE *stream = fopen(filename, "wb");
    ASSERT(stream);
    uint32_t result = (uint32_t)fwrite(data, 1, size, stream);
    fclose(stream);

    return result;
}

template<typename T>
struct BufferView
{
    const T *data;
    uint32_t length;
};

inline BufferView<char> getFilename(const char *path, bool withExtension)
{
    const char *lastSlash = strrchr(path, '/');
    path = lastSlash ? lastSlash + 1 : path;
    const char *lastDot = withExtension ? nullptr : strrchr(path, '.');
    uint32_t length = (uint32_t)(lastDot ? (lastDot - path) : strlen(path));

    return { path, length };
}