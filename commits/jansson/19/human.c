int json_array_remove(json_t *json, size_t index)
{
    json_array_t *array;

    if(!json_is_array(json))
        return -1;
    array = json_to_array(json);

    if(index >= array->entries)
        return -1;

    json_decref(array->table[index]);

    /* If we're removing the last element, nothing has to be moved */
    if(index < array->entries - 1)
        array_move(array, index, index + 1, array->entries - index - 1);

    array->entries--;

    return 0;
}