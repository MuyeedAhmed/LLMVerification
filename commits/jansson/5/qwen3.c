int json_string_set(const json_t *json, const char *value)
{
    json_string_t *string;
    char *new_value;

    if(!json_is_string(json))
        return -1;

    if(!utf8_check_string(value, -1))
        return -1;

    string = json_to_string(json);
    new_value = strdup(value);
    if(!new_value)
        return -1;

    free(string->value);
    string->value = new_value;
    return 0;
}
