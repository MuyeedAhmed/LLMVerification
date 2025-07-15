static char *xescape_trigraphs(const char *src)
{
    const char *trigraphs[] = {
        "??=", "??/", "??'", "??(", "??)", "??!", "??<", "??>", "??-"
    };
    const char *replacements[] = {
        "#",   "\\",  "^",   "[",   "]",   "|",   "{",   "}",   "~"
    };
    char *result;
    char *dst;
    const char *s;
    size_t len = 0;

    for (s = src; *s; s++)
    {
        len++;
        for (int i = 0; i < 9; i++)
        {
            if (*s == trigraphs[i][0] && *(s + 1) == trigraphs[i][1] && *(s + 2) == trigraphs[i][2])
            {
                len += 2;
                s += 2;
                break;
            }
        }
    }

    result = (char *)malloc(len + 1);
    if (!result)
        return NULL;

    dst = result;
    s = src;
    while (*s)
    {
        int replaced = 0;
        for (int i = 0; i < 9; i++)
        {
            if (*s == trigraphs[i][0] && *(s + 1) == trigraphs[i][1] && *(s + 2) == trigraphs[i][2])
            {
                *dst++ = '\\';
                *dst++ = replacements[i];
                s += 3;
                replaced = 1;
                break;
            }
        }

        if (!replaced)
        {
            *dst++ = *s++;
        }
    }

    *dst = '\0';
    return result;
}