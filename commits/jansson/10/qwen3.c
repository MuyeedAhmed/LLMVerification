int json_equal(json_t *json1, json_t *json2)
{
    if (!json1 || !json2)
        return 0;

    if (json1 == json2)
        return 1;

    if (json_typeof(json1) != json_typeof(json2))
        return 0;

    switch (json_typeof(json1)) {
        case JSON_OBJECT: {
            unsigned int size1 = json_object_size(json1);
            unsigned int size2 = json_object_size(json2);

            if (size1 != size2)
                return 0;

            void *iter1 = json_object_iter(json1);
            while (iter1) {
                const char *key = json_object_iter_key(iter1);
                json_t *value1 = json_object_iter_value(iter1);
                json_t *value2 = json_object_get(json2, key);

                if (!value2 || !json_equal(value1, value2))
                    return 0;

                iter1 = json_object_iter_next(json1, iter1);
            }
            return 1;
        }

        case JSON_ARRAY: {
            unsigned int size1 = json_array_size(json1);
            unsigned int size2 = json_array_size(json2);

            if (size1 != size2)
                return 0;

            for (unsigned int i = 0; i < size1; i++) {
                json_t *value1 = json_array_get(json1, i);
                json_t *value2 = json_array_get(json2, i);

                if (!json_equal(value1, value2))
                    return 0;
            }
            return 1;
        }

        case JSON_STRING:
            return strcmp(json_string_value(json1), json_string_value(json2)) == 0;

        case JSON_INTEGER:
            return json_integer_value(json1) == json_integer_value(json2);

        case JSON_REAL:
            return json_real_value(json1) == json_real_value(json2);

        case JSON_TRUE:
        case JSON_FALSE:
        case JSON_NULL:
            return 1;

        default:
            return 0;
    }
}
