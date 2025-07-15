int json_string_set_nocheck(json_t *json, const char *value)
{
    char *dup;
    json_string_t *string;

    if (!json_is_string(json) || !value) // Check if target is a string and value is not NULL
        return -1;

    dup = jsonp_strdup(value);
    if (!dup)
        return -1;

    string = json_to_string(json);
    jsonp_free(string->value);
    string->value = dup;

    return 0;
}