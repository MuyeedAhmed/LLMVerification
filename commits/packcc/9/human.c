static const char *extract_filename(const char *path) {
    size_t i = strlen(path);
    while (i > 0) {
        i--;
#ifdef _WIN32
        if (strchr("/\\:", path[i])) return path + i + 1;
#else
        if (path[i] == '/') return path + i + 1;
#endif
    }
    return path;
}