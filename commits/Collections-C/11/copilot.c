/**
 * Reverses the order of elements in the specified array.
 *
 * @param[in] ar array that is being reversed
 */
void array_reverse(Array *ar)
{
    if (ar->size == 0) {
        // Do nothing if the array is empty
        return;
    }

    size_t i;
    size_t j;
    for (i = 0, j = ar->size - 1; i < j; i++, j--) {
        void *tmp = ar->buffer[i];
        ar->buffer[i] = ar->buffer[j];
        ar->buffer[j] = tmp;
    }
}