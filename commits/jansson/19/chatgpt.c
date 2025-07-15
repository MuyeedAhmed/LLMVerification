int json_array_remove(json_t *json, size_t index)
{
    json_array_t *array;

    if (!json_is_array(json))
        return -1;

    array = json_to_array(json);

    if (index >= array->entries)
        return -1;

    // Decrement the reference count of the element being removed
    json_decref(array->table[index]);

    // Move the elements after the removed element one position to the left
    array_move(array, index, index + 1, array->entries - index - 1);

    // Decrease the number of entries
    array->entries--;

    return 0;
}