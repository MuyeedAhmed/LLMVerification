int json_object_update_missing(json_t *object, json_t *other)
{
    void *iter;

    if (!json_is_object(object) || !json_is_object(other))
        return -1;

    iter = json_object_iter(other);
    while (iter) {
        const char *key = json_object_iter_key(iter);
        json_t *value = json_object_iter_value(iter);

        // Only add the key-value pair if the key does not already exist in the object
        if (!json_object_get(object, key)) {
            if (json_object_set_nocheck(object, key, value) == -1)
                return -1;
        }

        iter = json_object_iter_next(other, iter);
    }

    return 0;
}