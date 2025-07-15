int json_object_clear(json_t *json)
{
    json_object_t *object;

    if (!json_is_object(json))
        return -1;

    object = json_to_object(json);

    // Clear the hashtable, decrementing the reference count of all values
    hashtable_clear(&object->hashtable);

    return 0;
}