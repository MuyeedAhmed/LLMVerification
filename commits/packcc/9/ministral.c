static const char *extract_filename(const char *path) {
    size_t i = strlen(path);
    while (i > 0) {
        i--;
        if (path[i] == '/' || path[i] == '\\' || path[i] == ':') return path + i + 1;
    }
    return path;
}
