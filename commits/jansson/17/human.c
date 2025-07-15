int json_object_update_missing(json_t *object, json_t *other)
{
    const char *key;
    json_t *value;

    if(!json_is_object(object) || !json_is_object(other))
        return -1;

    json_object_foreach(other, key, value) {
        if(!json_object_get(object, key))
            json_object_set_nocheck(object, key, value);
    }

    return 0;
}