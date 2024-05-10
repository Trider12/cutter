#pragma once

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ASSERT assert

#if defined _DEBUG || !defined NDEBUG
#define DEBUG
#endif

#ifdef DEBUG
#define VERIFY ASSERT
#else
#define VERIFY(expr) (void)(expr)
#endif

#ifdef _MSC_VER
#define COUNTOF _countof
#include <io.h>
#define ACCESS _access
#else
#define COUNTOF(arr) sizeof(arr) / sizeof((arr)[0])
#include <unistd.h>
#define ACCESS access
#endif

#define EXISTS(path) !ACCESS(path, 0)

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