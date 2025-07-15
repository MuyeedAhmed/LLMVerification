enum cc_stat array_get_last(Array *ar, void **out)
{
    if (ar->size == 0) {
        // Array is empty, return an error
        return CC_ERR_OUT_OF_RANGE;
    }

    // Retrieve the last element
    *out = ar->buffer[ar->size - 1];
    return CC_OK;
}