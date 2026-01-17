enum cc_stat cc_queue_new_conf(CC_QueueConf const * const conf, CC_Queue **q)
{
    if (!conf || !q)
        return CC_ERR_INVALID_INPUT;
    
    CC_Queue *queue = conf->mem_calloc(1, sizeof(CC_Queue));
 
    if (!queue)
        return CC_ERR_ALLOC;
 
    CC_Deque *deque;
    enum cc_stat stat = cc_deque_new_conf(conf, &deque);
 
    if (stat != CC_OK) {
        conf->mem_free(queue);
        return stat;
    }
 
    queue->d          = deque;
    queue->mem_alloc  = conf->mem_alloc;
    queue->mem_calloc = conf->mem_calloc;
    queue->mem_free   = conf->mem_free;
 
    *q = queue;
 
    return CC_OK;
}
