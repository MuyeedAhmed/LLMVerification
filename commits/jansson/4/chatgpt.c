int json_object_update(json_t *object, json_t *other)
{
    void *iter;
    const char *key;
    json_t *value;

    if (!json_is_object(object) || !json_is_object(other))
        return -1;

    iter = json_object_iter(other);
    while (iter) {
        key = json_object_iter_key(iter);
        value = json_object_iter_value(iter);

        if (json_object_set(object, key, value) == -1)
            return -1;

        iter = json_object_iter_next(other, iter);
    }

    return 0;
}