char *jsonp_strdup(const char *str)
{
    char *new_str;
    size_t len;

    len = strlen(str);
    if(len == (size_t)-1)
        return NULL;

    new_str = jsonp_malloc(len + 1);
    if(!new_str)
        return NULL;

    memcpy(new_str, str, len + 1);
    return new_str;
}