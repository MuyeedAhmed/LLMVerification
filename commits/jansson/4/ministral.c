int json_object_update(json_t *object, json_t *other) {
    if (!json_is_object(object) || !json_is_object(other)) {
        return -1;
    }

    json_object_t *obj = json_to_object(object);
    json_object_t *oth = json_to_object(other);

    void *iter = hashtable_iter(&oth->hashtable);
    while (iter) {
        const char *key = hashtable_iter_key(iter);
        json_t *value = (json_t *)hashtable_iter_value(iter);

        if (hashtable_set(&obj->hashtable, strdup(key), json_incref(value)) != 0) {
            json_decref(value);
            return -1;
        }

        iter = hashtable_iter_next(&oth->hashtable, iter);
    }

    return 0;
}
