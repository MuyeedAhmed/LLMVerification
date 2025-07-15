enum cc_stat cc_deque_remove_at(CC_Deque *deque, size_t index, void **out)
{
    if (index >= deque->size)
        return CC_ERR_OUT_OF_RANGE;

    const size_t c = deque->capacity - 1;
    const size_t l = deque->last & c;
    const size_t f = deque->first & c;
    const size_t p = (deque->first + index) & c;

    void *removed  = deque->buffer[p];

    if (index == 0)
        return cc_deque_remove_first(deque, out);

    if (index == c)
        return cc_deque_remove_last(deque, out);

    if (index <= (deque->size / 2) - 1) {
        if (p < f) {
            void *e = deque->buffer[c];

            if (f != c) {
                memmove(&(deque->buffer[f + 1]),
                        &(deque->buffer[f]),
                        (c - f) * sizeof(void*));
            }
            if (p != 0) {
                memmove(&(deque->buffer[1]),
                        &(deque->buffer[0]),
                        p * sizeof(void*));
            }
            deque->buffer[0] = e;
        } else {
            memmove(&(deque->buffer[f + 1]),
                    &(deque->buffer[f]),
                    index * sizeof(void*));
        }
        deque->first = (deque->first + 1) & c;
    } else {
        if (p > l) {
            void *e = deque->buffer[0];

            if (p != c) {
                memmove(&(deque->buffer[p]),
                        &(deque->buffer[p + 1]),
                        (c - p) * sizeof(void*));
            }
            if (p != 0) {
                memmove(&(deque->buffer[1]),
                        &(deque->buffer[0]),
                        l * sizeof(void*));
            }
            deque->buffer[c] = e;
        } else {
            memmove(&(deque->buffer[p]),
                    &(deque->buffer[p + 1]),
                    (l - p) * sizeof(void*));
        }
        deque->last = (deque->last- 1) & c;
    }
    deque->size--;

    if (out)
        *out = removed;
    return CC_OK;
}