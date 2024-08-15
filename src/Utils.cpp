#include "Utils.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#include <io.h> // for _access
#include <direct.h> // for _mkdir
#else // assume UNIX
#include <unistd.h> // for access
#include <sys/stat.h> // for mkdir
#endif

#include <cwalk.h>

bool mkdir(const char *path, bool recursive)
{
    ASSERT(isValidString(path));

    if (!recursive)
    {
#ifdef _MSC_VER
        return !_mkdir(path);
#else // assume UNIX
        return !mkdir(path, 0777);
#endif
    }

    char buffer[256];
    uint8_t offset = 0;
    cwk_segment segment;
    VERIFY(cwk_path_get_first_segment(path, &segment));
    cwk_segment lastSegment;
    VERIFY(cwk_path_get_last_segment(path, &lastSegment));

    do
    {
        offset += (uint8_t)snprintf(buffer + offset, sizeof(buffer) - offset, "%.*s/", (uint32_t)segment.size, segment.begin);
#ifdef _MSC_VER
        _mkdir(buffer);
#else // assume UNIX
        mkdir(dirname, 0777);
#endif
    } while (cwk_path_get_next_segment(&segment));

    return true;
}

bool pathExists(const char *path)
{
    ASSERT(isValidString(path));
#ifdef _MSC_VER
    return !_access(path, 0);
#else // assume UNIX
    return !access(path, 0);
#endif
}

uint32_t readFile(const char *filename, uint8_t *buffer, uint32_t bufferSize)
{
    ASSERT(isValidString(filename));
    ASSERT(pathExists(filename));
    FILE *stream = fopen(filename, "rb");
    ASSERT(stream);
    fseek(stream, 0, SEEK_END);
    uint32_t size = ftell(stream);

    if (buffer)
    {
        VERIFY(bufferSize >= size);
        fseek(stream, 0, SEEK_SET);
        size = (uint32_t)fread(buffer, 1, size, stream);
    }

    fclose(stream);

    return size;
}

uint32_t writeFile(const char *filename, const void *data, uint32_t size)
{
    ASSERT(isValidString(filename));
    ASSERT(data);
    size_t dirnameLength;
    cwk_path_get_dirname(filename, &dirnameLength);

    if (dirnameLength > 0)
    {
        char dirname[256];
        ASSERT(dirnameLength < sizeof(dirname));
        snprintf(dirname, sizeof(dirname), "%.*s", (uint32_t)dirnameLength, filename);
        mkdir(dirname, true);
    }

    FILE *stream = fopen(filename, "wb");
    ASSERT(stream);
    uint32_t result = (uint32_t)fwrite(data, 1, size, stream);
    fclose(stream);

    return result;
}