static void pqueue_heapify(PQueue *pq, size_t index)
{
    if (pq->size <= 1)
        return;

    size_t L   = CC_LEFT(index);
    size_t R   = CC_RIGHT(index);
    size_t tmp = index;

    void *left     = pq->buffer[L];
    void *right    = pq->buffer[R];
    void *indexPtr = pq->buffer[index];

    if (L < pq->size && pq->cmp(indexPtr, left) < 0) {
        indexPtr = left;
        index = L;
    }

    if (R < pq->size && pq->cmp(indexPtr, right) < 0) {
        indexPtr = right;
        index = R;
    }

    if (index != tmp) {
        void *swap_tmp = pq->buffer[tmp];
        pq->buffer[tmp] = pq->buffer[index];
        pq->buffer[index] = swap_tmp;

        pqueue_heapify(pq, index);
    }
}