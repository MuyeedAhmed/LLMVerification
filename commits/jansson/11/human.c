json_t *json_copy(json_t *json)
{
    if(!json)
        return NULL;

    if(json_is_object(json))
        return json_object_copy(json);

    if(json_is_array(json))
        return json_array_copy(json);

    if(json_is_string(json))
        return json_string_copy(json);

    if(json_is_integer(json))
        return json_integer_copy(json);

    if(json_is_real(json))
        return json_real_copy(json);

    if(json_is_true(json) || json_is_false(json) || json_is_null(json))
        return json;

    return NULL;
}