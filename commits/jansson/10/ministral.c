int json_equal(json_t *json1, json_t *json2) {
    if (!json1 || !json2) {
        return 0;
    }

    if (json1->type != json2->type) {
        return 0;
    }

    switch (json1->type) {
        case JSON_NULL:
        case JSON_TRUE:
        case JSON_FALSE:
            return 1;

        case JSON_INTEGER:
            return json_integer_value(json1) == json_integer_value(json2);

        case JSON_REAL:
            return json_real_value(json1) == json_real_value(json2);

        case JSON_STRING:
            return strcmp(json_string_value(json1), json_string_value(json2)) == 0;

        case JSON_ARRAY: {
            json_array_t *array1 = json_to_array(json1);
            json_array_t *array2 = json_to_array(json2);

            if (array1->entries != array2->entries) {
                return 0;
            }

            for (unsigned int i = 0; i < array1->entries; i++) {
                if (!json_equal(array1->table[i], array2->table[i])) {
                    return 0;
                }
            }
            return 1;
        }

        case JSON_OBJECT: {
            json_object_t *object1 = json_to_object(json1);
            json_object_t *object2 = json_to_object(json2);

            void *iter1 = json_object_iter(json1);
            void *iter2 = json_object_iter(json2);

            while (iter1) {
                const char *key = json_object_iter_key(iter1);
                json_t *value1 = json_object_iter_value(iter1);

                json_t *value2 = json_object_get(json2, key);
                if (!value2 || !json_equal(value1, value2)) {
                    return 0;
                }

                iter1 = json_object_iter_next(json1, iter1);
            }

            while (iter2) {
                const char *key = json_object_iter_key(iter2);
                json_t *value2 = json_object_iter_value(iter2);

                json_t *value1 = json_object_get(json1, key);
                if (!value1) {
                    return 0;
                }

                iter2 = json_object_iter_next(json2, iter2);
            }

            return 1;
        }

        default:
            return 0;
    }
}
