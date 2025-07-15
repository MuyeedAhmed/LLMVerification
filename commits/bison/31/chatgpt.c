static ptrdiff_t escape_trigraphs(char *buffer, ptrdiff_t buffersize, const char *src) {
    static const struct {
        const char *trigraph;
        char replacement;
    } trigraphs[] = {
        {"??=", '#'}, {"??/", '\\'}, {"??'", '^'}, {"??(", '['},
        {"??)", ']'}, {"??!", '|'}, {"??<", '{'}, {"??>", '}'}, {"??-", '~'}
    };
    const size_t trigraph_count = sizeof(trigraphs) / sizeof(trigraphs[0]);

    ptrdiff_t required_size = 1; // For the null terminator

    for (const char *p = src; *p; ++p) {
        if (*p == '?' && *(p + 1) == '?') {
            int found = 0;
            for (size_t i = 0; i < trigraph_count; ++i) {
                if (strncmp(p, trigraphs[i].trigraph, 3) == 0) {
                    required_size++;
                    p += 2; // Skip the trigraph
                    found = 1;
                    break;
                }
            }
            if (!found) {
                required_size++;
            }
        } else {
            required_size++;
        }

        if (buffersize && required_size <= buffersize) {
            if (*p == '?' && *(p + 1) == '?') {
                int found = 0;
                for (size_t i = 0; i < trigraph_count; ++i) {
                    if (strncmp(p, trigraphs[i].trigraph, 3) == 0) {
                        *buffer++ = trigraphs[i].replacement;
                        p += 2; // Skip the trigraph
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    *buffer++ = *p;
                }
            } else {
                *buffer++ = *p;
            }
        }
    }

    if (buffersize && required_size <= buffersize) {
        *buffer = '\0';
    }

    return required_size;
}