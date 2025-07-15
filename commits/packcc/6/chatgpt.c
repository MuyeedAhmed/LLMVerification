#include <stddef.h>

typedef struct char_array_tag {
    char *buf;
    size_t max;
    size_t len;
} char_array_t;

static void char_array__init(char_array_t *array, size_t max) {
    array->len = 0;
    array->max = max;
    array->buf = NULL; // No memory allocation during initialization
}