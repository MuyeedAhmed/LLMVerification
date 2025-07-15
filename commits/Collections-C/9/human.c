enum cc_stat array_remove_at(Array *ar, size_t index, void **out)
{
    if (index >= ar->size)
        return CC_ERR_OUT_OF_RANGE;

    if (out)
        *out = ar->buffer[index];

    if (index != ar->size - 1) {
        size_t block_size = (ar->size - 1 - index) * sizeof(void*);

        memmove(&(ar->buffer[index]),
                &(ar->buffer[index + 1]),
                block_size);
    }
    ar->size--;

    return CC_OK;
}