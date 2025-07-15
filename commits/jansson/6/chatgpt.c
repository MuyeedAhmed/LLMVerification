int json_integer_set(json_t *json, int value)
{
    json_integer_t *integer;

    if (!json_is_integer(json))
        return -1;

    integer = json_to_integer(json);
    integer->value = value;

    return 0;
}