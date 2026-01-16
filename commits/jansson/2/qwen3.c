unsigned int json_object_size(const json_t *json)
{
    if (!json_is_object(json))
        return 0;

    const json_object_t *object = json_to_object(json);
    return object->hashtable.entries;
}
