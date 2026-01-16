size_t json_string_length(const json_t *string)
{
    if (!json_is_string(string))
        return 0;

    const char *str_value = json_string_value(string);
    if (str_value == NULL)
        return 0;

    return strlen(str_value);
}
