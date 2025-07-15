int json_object_set_new_nocheck(json_t *json, const char *key, json_t *value)
{
    json_object_t *object;
    object_key_t *k;

    if(!key || !value)
        return -1;

    if(!json_is_object(json) || json == value)
    {
        json_decref(value);
        return -1;
    }
    object = json_to_object(json);

    /* offsetof(...) returns the size of object_key_t without the
       last, flexible member. This way, the correct amount is
       allocated. */
    k = jsonp_malloc(offsetof(object_key_t, key) + strlen(key) + 1);
    if(!k)
    {
        json_decref(value);
        return -1;
    }

    k->serial = object->serial++;
    strcpy(k->key, key);

    if(hashtable_set(&object->hashtable, k, value))
    {
        json_decref(value);
        return -1;
    }

    return 0;
}