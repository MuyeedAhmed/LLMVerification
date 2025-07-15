/**
 * Reverses the order of elements in the specified array.
 *
 * @param[in] ar array that is being reversed
 */
void cc_array_reverse(CC_Array *ar)
{
    if (ar->size == 0)
        return;

    size_t i;
    size_t j;
    for (i = 0, j = ar->size - 1; i < j; i++, j--) {
        void *tmp = ar->buffer[i];
        ar->buffer[i] = ar->buffer[j];
        ar->buffer[j] = tmp;
    }
}