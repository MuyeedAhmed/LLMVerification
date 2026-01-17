enum cc_stat cc_queue_new_conf(CC_QueueConf const * const conf, CC_Queue **q)
{
    CC_Queue *queue = conf->mem_calloc(1, sizeof(CC_Queue));

    if (!queue)
        return CC_ERR_ALLOC;

    CC_Deque *deque = NULL;
    enum cc_stat ret = cc_deque_new_conf(conf, &deque);

    if (ret != CC_OK) {
        conf->mem_free(queue);
        return ret;
    }

    queue->d          = deque;
    queue->mem_alloc  = conf->mem_alloc;
    queue->mem_calloc = conf->mem_calloc;
    queue->mem_free   = conf->mem_free;

    *q = queue;

    return CC_OK;
}
