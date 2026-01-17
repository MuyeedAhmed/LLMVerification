static void pqueue_heapify(PQueue *pq, size_t index)
{
    if (pq->size <= 1)
        return;

    size_t L = CC_LEFT(index);
    size_t R = CC_RIGHT(index);
    size_t smallest = index;

    void *left = pq->buffer[L];
    void *right = pq->buffer[R];
    void *current = pq->buffer[index];

    if (L < pq->size && pq->cmp(current, left) > 0)
        smallest = L;

    if (R < pq->size && pq->cmp(current, right) > 0)
        smallest = R;

    if (smallest != index) {
        void *swap_tmp = pq->buffer[index];
        pq->buffer[index] = pq->buffer[smallest];
        pq->buffer[smallest] = swap_tmp;

        pqueue_heapify(pq, smallest);
    }
}
