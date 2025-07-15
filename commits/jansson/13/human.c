void *json_object_iter_at(json_t *json, const char *key)
{
    json_object_t *object;

    if(!key || !json_is_object(json))
        return NULL;

    object = json_to_object(json);
    return hashtable_iter_at(&object->hashtable, string_to_key(key));
}