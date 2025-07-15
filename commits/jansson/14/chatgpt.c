int json_object_set_new_nocheck(json_t *json, const char *key, json_t *value)
{
    json_object_t *object;
    object_key_t *k;

    if (!key || !value)
        return -1;

    if (!json_is_object(json) || json == value)
    {
        json_decref(value);
        return -1;
    }

    object = json_to_object(json);

    /* Allocate memory for the key */
    k = jsonp_malloc(offsetof(object_key_t, key) + strlen(key) + 1);
    if (!k)
    {
        json_decref(value); // Decrement reference count to avoid memory leak
        return -1;
    }

    k->serial = object->serial++;
    strcpy(k->key, key);

    /* Attempt to set the key-value pair in the hashtable */
    if (hashtable_set(&object->hashtable, k, value))
    {
        jsonp_free(k);      // Free the allocated key memory
        json_decref(value); // Decrement reference count to avoid memory leak
        return -1;
    }

    return 0;
}