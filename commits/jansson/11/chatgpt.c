json_t *json_copy(json_t *json)
{
    if (!json)
        return NULL;

    switch (json_typeof(json)) {
        case JSON_OBJECT:
            return json_object_copy(json);

        case JSON_ARRAY:
            return json_array_copy(json);

        case JSON_STRING:
            return json_string_copy(json);

        case JSON_INTEGER:
            return json_integer_copy(json);

        case JSON_REAL:
            return json_real_copy(json);

        case JSON_TRUE:
            return json_true();

        case JSON_FALSE:
            return json_false();

        case JSON_NULL:
            return json_null();

        default:
            return NULL;
    }
}