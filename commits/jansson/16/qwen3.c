int json_object_update_existing(json_t *object, json_t *other)
{
    void *iter;

    if(!json_is_object(object) || !json_is_object(other))
        return -1;

    iter = json_object_iter(other);
    while(iter) {
        const char *key;
        json_t *value;

        key = json_object_iter_key(iter);
        value = json_object_iter_value(iter);

        /* Only update existing keys, don't add new ones */
        if(json_object_get(object, key) != NULL) {
            if(json_object_set_nocheck(object, key, value))
                return -1;
        }

        iter = json_object_iter_next(other, iter);
    }

    return 0;
}
