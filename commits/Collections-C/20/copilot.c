/**
 * Creates a new empty CC_Queue based on the specified CC_QueueConf object and
 * returns a status code.
 *
 * The CC_Queue is allocated using the allocators specified in the CC_QueueConf struct.
 * The allocation may fail if the underlying allocator fails.
 *
 * @param[in] conf CC_Queue configuration structure. All fields must be initialized
 *                 with appropriate values.
 * @param[out] q Pointer to where the newly created CC_Queue is to be stored.
 *
 * @return CC_OK if the creation was successful, or CC_ERR_ALLOC if the memory
 * allocation for the new CC_Queue structure failed.
 */
enum cc_stat cc_queue_new_conf(CC_QueueConf const * const conf, CC_Queue **q)
{
    CC_Queue *queue = conf->mem_calloc(1, sizeof(CC_Queue));

    if (!queue)
        return CC_ERR_ALLOC;

    queue->d = NULL; // Initialize to NULL to avoid uninitialized memory access

    CC_Deque *deque;
    enum cc_stat status = cc_deque_new_conf(conf, &deque);

    if (status != CC_OK) {
        conf->mem_free(queue);
        return status;
    }

    queue->d          = deque;
    queue->mem_alloc  = conf->mem_alloc;
    queue->mem_calloc = conf->mem_calloc;
    queue->mem_free   = conf->mem_free;

    *q = queue;

    return CC_OK;
}