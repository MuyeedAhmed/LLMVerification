unsigned int json_object_size(const json_t *json)
{
    if (!json_is_object(json))
        return 0;

    json_object_t *object = json_to_object(json);
    return hashtable_size(&object->hashtable);
}