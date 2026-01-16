json_t *json_copy(json_t *json)
{
    if(!json)
        return NULL;

    switch(json->type) {
        case JSON_OBJECT: {
            json_t *copy = json_object();
            if(!copy)
                return NULL;

            void *iter = json_object_iter(json);
            while(iter) {
                const char *key = json_object_iter_key(iter);
                json_t *value = json_object_iter_value(iter);
                if(json_object_set_new_nocheck(copy, key, value)) {
                    json_decref(copy);
                    return NULL;
                }
                iter = json_object_iter_next(json, iter);
            }
            return copy;
        }

        case JSON_ARRAY: {
            json_t *copy = json_array();
            if(!copy)
                return NULL;

            unsigned int i;
            for(i = 0; i < json_array_size(json); i++) {
                json_t *value = json_array_get(json, i);
                if(json_array_append_new(copy, value)) {
                    json_decref(copy);
                    return NULL;
                }
            }
            return copy;
        }

        case JSON_STRING:
            return json_string_nocheck(json_string_value(json));

        case JSON_INTEGER:
            return json_integer(json_integer_value(json));

        case JSON_REAL:
            return json_real(json_real_value(json));

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
