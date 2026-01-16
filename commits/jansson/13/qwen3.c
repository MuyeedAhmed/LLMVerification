void *json_object_iter_at(json_t *json, const char *key)
{
    void *iter;
    const char *iter_key;
    json_object_t *object;

    if(!json_is_object(json) || !key)
        return NULL;

    object = json_to_object(json);
    iter = hashtable_iter(&object->hashtable);
    while(iter) {
        iter_key = (const char *)hashtable_iter_key(iter);
        if(strcmp(iter_key, key) == 0)
            return iter;
        iter = hashtable_iter_next(&object->hashtable, iter);
    }

    return NULL;
}
