static ptrdiff_t
escape_trigraphs (char *buffer, ptrdiff_t buffersize, const char *src)
{
    ptrdiff_t result_size = 0;
    const char *p = src;

    while (*p) {
        if (p[0] == '?' && p[1] == '?' && p[2] == '=') {
            if (buffersize > 0) {
                if (result_size + 2 >= buffersize) {
                    return -1;
                }
                buffer[result_size++] = '?';
                buffer[result_size++] = '?';
                buffer[result_size++] = '/';
                p += 3;
            } else {
                result_size += 2;
            }
        } else {
            if (buffersize > 0) {
                if (result_size >= buffersize) {
                    return -1;
                }
                buffer[result_size++] = *p;
            }
            p++;
        }
    }

    if (buffersize > 0) {
        if (result_size >= buffersize) {
            return -1;
        }
        buffer[result_size] = '\0';
    }

    return result_size + 1;
}
