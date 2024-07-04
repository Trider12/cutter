#pragma once

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

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
#include <malloc.h> // for _alloca
#define alloca _alloca
#else // assume UNIX
#define COUNTOF(arr) sizeof(arr) / sizeof((arr)[0])
#include <alloca.h> // for alloca
#endif

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

bool mkdir(const char *path, bool recursive);

bool pathExists(const char *path);

uint32_t readFile(const char *filename, uint8_t *buffer, uint32_t bufferSize);

uint32_t writeFile(const char *filename, const void *data, uint32_t size);