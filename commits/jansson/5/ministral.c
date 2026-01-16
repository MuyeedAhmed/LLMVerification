int json_string_set(const json_t *json, const char *value)
{
    json_string_t *string;

    if (!json_is_string(json))
        return -1;

    string = json_to_string(json);

    free(string->value);
    string->value = strdup(value);

    if (!string->value)
        return -1;

    return 0;
}
