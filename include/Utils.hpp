#pragma once

#include <assert.h>
#include <stdint.h>
#ifdef _MSC_VER
#include <malloc.h> // for alloca
#else // assume UNIX
#include <alloca.h> // for alloca
#endif

#define ASSERT assert

#ifdef DEBUG
#define VERIFY ASSERT
#else
#define VERIFY(expr) (void)(expr)
#endif

#define UNUSED(var) (void)(var)

#define NAMEOF(var) #var

#define CPU_PAUSE _mm_pause // TODO: handle non x64

#define defineEnumOperators(enumType, intType) \
inline intType  operator+(enumType a) { return (intType)a; } \
inline enumType operator~(enumType a) { return (enumType)(~(intType)a); } \
inline enumType operator&(enumType a, enumType b) { return (enumType)((intType)a & (intType)b); } \
inline enumType operator|(enumType a, enumType b) { return (enumType)((intType)a | (intType)b); } \
inline intType  operator&(enumType a, intType b) { return (intType)a & b; } \
inline intType  operator|(enumType a, intType b) { return (intType)a | b; } \
inline intType  operator&(intType a, enumType b) { return a & (intType)b; } \
inline intType  operator|(intType a, enumType b) { return a | (intType)b; }

#define EnumBool(enumType) enum class enumType : uint8_t { No = 0, Yes = 1 }

template <typename T, uint32_t N>
inline constexpr uint32_t countOf(T(&)[N]) { return N; }

inline constexpr uint32_t max(uint32_t a, uint32_t b)
{
    return a > b ? a : b;
}

inline constexpr uint32_t min(uint32_t a, uint32_t b)
{
    return a < b ? a : b;
}

inline constexpr bool isPowerOf2(uint32_t value)
{
    return value && !(value & (value - 1));
}

inline constexpr uint32_t aligned(uint32_t value, uint32_t alignment) // alignment must be a power of 2!
{
    ASSERT(isPowerOf2(alignment));
    return (value + alignment - 1) & ~(alignment - 1);
}

inline constexpr bool isValidString(const char *str)
{
    return str && *str;
}

bool mkdir(const char *path, bool recursive);

bool pathExists(const char *path);

uint32_t readFile(const char *filename, uint8_t *buffer, uint32_t bufferSize);

uint32_t writeFile(const char *filename, const void *data, uint32_t size);