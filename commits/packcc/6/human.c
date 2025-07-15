static void char_array__init(char_array_t *array) {
    array->len = 0;
    array->max = 0;
    array->buf = NULL;
}