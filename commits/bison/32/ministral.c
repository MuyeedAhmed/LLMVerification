static char *
xescape_trigraphs (const char *src)
{
    char *dst = xstrdup (src);
    char *p = dst;
    char *q = dst;

    while (*src)
    {
        if (*src == '?' && src[1] == '?' && src[2] == '?')
        {
            *p++ = '\\';
            *p++ = '?';
            src += 3;
        }
        else
        {
            *p++ = *src++;
        }
    }

    *p = '\0';
    return q;
}
