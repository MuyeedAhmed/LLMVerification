static const char *extract_filename(const char *path) {
    size_t i = strlen(path);
    while (i > 0) {
        i--;
        if (path[i] == '/') return path + i + 1;
#ifdef _WIN32
        if (path[i] == '\\' || path[i] == ':') return path + i + 1;
#endif
    }
    return path;
}
