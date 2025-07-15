enum cc_stat array_remove_at(Array *ar, size_t index, void **out)
{
    if (index >= ar->size) {
        // Ensure the index is within bounds
        return CC_ERR_OUT_OF_RANGE;
    }

    if (out) {
        // Store the removed element in the output parameter
        *out = ar->buffer[index];
    }

    if (index != ar->size - 1) {
        // Fix: Adjust the block size to correctly shift elements
        size_t block_size = (ar->size - index - 1) * sizeof(void*);

        memmove(&(ar->buffer[index]),
                &(ar->buffer[index + 1]),
                block_size);
    }

    ar->size--; // Decrease the size of the array

    return CC_OK;
}