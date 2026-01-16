char *jsonp_strdup(const char *str)
{
    char *new_str;
    size_t len;

    if(!str)
        return NULL;

    len = strlen(str);
    if(len == SIZE_MAX)
        return NULL;

    new_str = jsonp_malloc(len + 1);
    if(!new_str)
        return NULL;

    strcpy(new_str, str);
    return new_str;
}
