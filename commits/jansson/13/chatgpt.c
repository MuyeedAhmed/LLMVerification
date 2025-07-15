void *json_object_iter_at(json_t *json, const char *key)
{
    json_object_t *object;

    if (!json_is_object(json) || !key)
        return NULL;

    object = json_to_object(json);
    return hashtable_iter_at(&object->hashtable, key);
}