int json_equal(json_t *json1, json_t *json2)
{
    if (!json1 || !json2)
        return 0;

    if (json_typeof(json1) != json_typeof(json2))
        return 0;

    switch (json_typeof(json1)) {
        case JSON_OBJECT:
            return json_object_equal(json1, json2);

        case JSON_ARRAY:
            return json_array_equal(json1, json2);

        case JSON_STRING:
            return json_string_equal(json1, json2);

        case JSON_INTEGER:
            return json_integer_equal(json1, json2);

        case JSON_REAL:
            return json_real_equal(json1, json2);

        case JSON_TRUE:
        case JSON_FALSE:
        case JSON_NULL:
            return 1;

        default:
            return 0;
    }
}