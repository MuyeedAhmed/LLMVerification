enum cc_stat rbuf_conf_new(RbufConf *rconf, Rbuf **rbuf)
{
    Rbuf *ringbuf = rconf->mem_calloc(1, sizeof(Rbuf));
    if (!ringbuf)
       return CC_ERR_ALLOC;
    if (!(ringbuf->buf = rconf->mem_calloc(rconf->capacity, sizeof(uint64_t)))) {
      rconf->mem_free(ringbuf);
      return CC_ERR_ALLOC;
    }

    ringbuf->mem_alloc = rconf->mem_alloc;
    ringbuf->mem_calloc = rconf->mem_calloc;
    ringbuf->mem_free = rconf->mem_free;
    ringbuf->capacity = rconf->capacity;
    ringbuf->size = 0;
    ringbuf->head = 0, ringbuf->tail = 0;
    *rbuf = ringbuf;
    return CC_OK;
}