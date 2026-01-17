void array_reverse(Array *ar)
{
    size_t i;
    size_t j;
    if (ar->size == 0)
        return;
    for (i = 0, j = ar->size - 1; i < ar->size / 2; i++, j--) {
        void *tmp = ar->buffer[i];
        ar->buffer[i] = ar->buffer[j];
        ar->buffer[j] = tmp;
    }
}
