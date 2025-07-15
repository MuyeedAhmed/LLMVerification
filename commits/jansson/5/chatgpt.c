int json_string_set(json_t *json, const char *value)
{
    json_string_t *string;

    if (!json_is_string(json) || !utf8_check_string(value, -1))
        return -1;

    string = json_to_string(json);

    // Free the old value
    free(string->value);

    // Duplicate the new value
    string->value = strdup(value);
    if (!string->value)
        return -1;

    return 0;
}