size_t json_string_length(const json_t *string)
{
    if (!json_is_string(string))
        return 0;

    const char *value = json_string_value(string);
    if (!value)
        return 0;

    return utf8_strlen(value);
}
