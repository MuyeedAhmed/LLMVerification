size_t json_string_length(const json_t *string)
{
    if (!json_is_string(string))
        return 0;

    return strlen(json_string_value(string));
}