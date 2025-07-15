int json_integer_set(const json_t *json, int value)
{
    if(!json_is_integer(json))
        return -1;

    json_to_integer(json)->value = value;

    return 0;
}