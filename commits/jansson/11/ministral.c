json_t *json_copy(json_t *json)
{
    if (!json)
        return NULL;

    json_t *copy = NULL;

    switch (json->type) {
        case JSON_OBJECT: {
            json_object_t *obj = json_to_object(json);
            json_object_t *new_obj = malloc(sizeof(json_object_t));
            if (!new_obj)
                return NULL;

            json_init(&new_obj->json, JSON_OBJECT);
            if (hashtable_init(&new_obj->hashtable, hash_string, string_equal, free, value_decref)) {
                free(new_obj);
                return NULL;
            }

            void *iter = json_object_iter(json);
            while (iter) {
                const char *key = json_object_iter_key(iter);
                json_t *value = json_object_iter_value(iter);
                json_t *new_value = json_copy(value);
                if (!new_value) {
                    hashtable_close(&new_obj->hashtable);
                    free(new_obj);
                    return NULL;
                }
                if (hashtable_set(&new_obj->hashtable, strdup(key), new_value)) {
                    json_decref(new_value);
                    hashtable_close(&new_obj->hashtable);
                    free(new_obj);
                    return NULL;
                }
                iter = json_object_iter_next(json, iter);
            }
            copy = &new_obj->json;
            break;
        }
        case JSON_ARRAY: {
            json_array_t *arr = json_to_array(json);
            json_array_t *new_arr = malloc(sizeof(json_array_t));
            if (!new_arr)
                return NULL;

            json_init(&new_arr->json, JSON_ARRAY);
            new_arr->entries = arr->entries;
            new_arr->size = arr->size;
            new_arr->table = malloc(new_arr->size * sizeof(json_t *));
            if (!new_arr->table) {
                free(new_arr);
                return NULL;
            }

            for (unsigned int i = 0; i < arr->entries; i++) {
                json_t *new_value = json_copy(arr->table[i]);
                if (!new_value) {
                    for (unsigned int j = 0; j < i; j++)
                        json_decref(new_arr->table[j]);
                    free(new_arr->table);
                    free(new_arr);
                    return NULL;
                }
                new_arr->table[i] = new_value;
            }
            copy = &new_arr->json;
            break;
        }
        case JSON_STRING: {
            json_string_t *str = json_to_string(json);
            json_string_t *new_str = malloc(sizeof(json_string_t));
            if (!new_str)
                return NULL;

            json_init(&new_str->json, JSON_STRING);
            new_str->value = strdup(str->value);
            if (!new_str->value) {
                free(new_str);
                return NULL;
            }
            copy = &new_str->json;
            break;
        }
        case JSON_INTEGER: {
            json_integer_t *int_val = json_to_integer(json);
            json_integer_t *new_int = malloc(sizeof(json_integer_t));
            if (!new_int)
                return NULL;

            json_init(&new_int->json, JSON_INTEGER);
            new_int->value = int_val->value;
            copy = &new_int->json;
            break;
        }
        case JSON_REAL: {
            json_real_t *real_val = json_to_real(json);
            json_real_t *new_real = malloc(sizeof(json_real_t));
            if (!new_real)
                return NULL;

            json_init(&new_real->json, JSON_REAL);
            new_real->value = real_val->value;
            copy = &new_real->json;
            break;
        }
        case JSON_TRUE:
        case JSON_FALSE:
        case JSON_NULL: {
            copy = json_incref(json);
            break;
        }
        default:
            return NULL;
    }

    return copy;
}
