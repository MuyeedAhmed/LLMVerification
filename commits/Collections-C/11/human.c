void array_reverse(Array *ar)
{
    if (ar->size == 0)
        return;

    size_t i;
    size_t j;
    for (i = 0, j = ar->size - 1; i < (ar->size - 1) / 2; i++, j--) {
        void *tmp = ar->buffer[i];
        ar->buffer[i] = ar->buffer[j];
        ar->buffer[j] = tmp;
    }
}