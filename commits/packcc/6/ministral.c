static void char_array__init(char_array_t *array, size_t max) {
    array->len = 0;
    array->max = max;
    array->buf = NULL;
}
