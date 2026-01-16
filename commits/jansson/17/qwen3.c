int json_object_update_missing(json_t *object, json_t *other)
{
    void *iter;

    if(!json_is_object(object) || !json_is_object(other))
        return -1;

    iter = json_object_iter(other);
    while(iter) {
        const char *key;
        json_t *value;
        json_t *existing;

        key = json_object_iter_key(iter);
        value = json_object_iter_value(iter);
        existing = json_object_get(object, key);

        /* Only set the value if the key doesn't already exist */
        if(!existing) {
            if(json_object_set_nocheck(object, key, value))
                return -1;
        }

        iter = json_object_iter_next(other, iter);
    }

    return 0;
}
