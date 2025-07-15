int json_string_set(const json_t *json, const char *value)
{
    char *dup;
    json_string_t *string;

    if(!json_is_string(json) || !value || !utf8_check_string(value, -1))
        return -1;

    dup = strdup(value);
    if(!dup)
        return -1;

    string = json_to_string(json);
    free(string->value);
    string->value = dup;

    return 0;
}