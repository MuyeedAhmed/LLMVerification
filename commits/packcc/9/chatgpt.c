#include <stddef.h>
#include <string.h>

static const char *extract_filename(const char *path) {
    size_t i = strlen(path);
#ifdef _WIN32
    // On Windows, consider both '/' and '\\' as path separators
    while (i > 0) {
        i--;
        if (path[i] == '/' || path[i] == '\\' || path[i] == ':') {
            return path + i + 1;
        }
    }
#else
    // On non-Windows systems, consider only '/' as a path separator
    while (i > 0) {
        i--;
        if (path[i] == '/') {
            return path + i + 1;
        }
    }
#endif
    return path;
}