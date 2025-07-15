enum cc_stat deque_trim_capacity(Deque *deque)
{
    if (deque->capacity == deque->size)
        return CC_OK;

    size_t new_size = upper_pow_two(deque->size);

    if (new_size == deque->capacity)
        return CC_OK;

    void **new_buff = deque->mem_alloc(sizeof(void*) * new_size);

    if (!new_buff)
        return CC_ERR_ALLOC;

    copy_buffer(deque, new_buff, NULL);

    // Free the old buffer to prevent memory leaks
    deque->mem_free(deque->buffer);

    deque->buffer   = new_buff;
    deque->first    = 0;
    deque->last     = deque->size;
    deque->capacity = new_size;
    return CC_OK;
}

static enum cc_stat expand_capacity(Deque *deque)
{
    if (deque->capacity == MAX_POW_TWO)
        return CC_ERR_MAX_CAPACITY;

    size_t new_capacity = deque->capacity << 1;
    void **new_buffer = deque->mem_calloc(new_capacity, sizeof(void*));

    if (!new_buffer)
        return CC_ERR_ALLOC;

    copy_buffer(deque, new_buffer, NULL);

    // Free the old buffer to prevent memory leaks
    deque->mem_free(deque->buffer);

    deque->first    = 0;
    deque->last     = deque->size;
    deque->capacity = new_capacity;
    deque->buffer   = new_buffer;

    return CC_OK;
}