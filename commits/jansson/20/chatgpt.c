char *jsonp_strdup(const char *str)
{
    size_t len;
    char *new_str;

    if (!str)
        return NULL;

    len = strlen(str);
    if (len + 1 == 0) // Check for overflow in size calculation
        return NULL;

    new_str = jsonp_malloc(len + 1);
    if (!new_str)
        return NULL;

    memcpy(new_str, str, len + 1); // Use memcpy for safety
    return new_str;
}