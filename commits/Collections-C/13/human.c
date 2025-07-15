#define CC_PARENT(x)  ((x > 0) ? (x - 1) / 2 : 0)

enum cc_stat cc_pqueue_push(CC_PQueue *pq, void *element)
{
    size_t i = pq->size;

    if (i >= pq->capacity) {
        enum cc_stat status = expand_capacity(pq);
        if (status != CC_OK)
            return status;
    }

    pq->buffer[i] = element;
    pq->size++;

    if (i == 0)
        return CC_OK;

    void *child  = pq->buffer[i];
    void *parent = pq->buffer[CC_PARENT(i)];

    while (i != 0 && pq->cmp(child, parent) > 0) {
        void *tmp = pq->buffer[i];
        pq->buffer[i] = pq->buffer[CC_PARENT(i)];
        pq->buffer[CC_PARENT(i)] = tmp;

        i      = CC_PARENT(i);
        child  = pq->buffer[i];
        parent = pq->buffer[CC_PARENT(i)];
    }
    return CC_OK;
}