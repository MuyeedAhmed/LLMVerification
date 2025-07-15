/**
 * Maintains the heap property of the PQueue
 *
 * @param[in] pq the PQueue structure whose heap property is to be maintained
 * @param[in] index the index from where we need to apply this operation
 */
static void pqueue_heapify(PQueue *pq, size_t index)
{
    if (pq->size <= 1)
        return;

    size_t L = CC_LEFT(index);
    size_t R = CC_RIGHT(index);
    size_t largest = index;

    // Check if the left child exists and is greater than the current largest
    if (L < pq->size && pq->cmp(pq->buffer[L], pq->buffer[largest]) > 0) {
        largest = L;
    }

    // Check if the right child exists and is greater than the current largest
    if (R < pq->size && pq->cmp(pq->buffer[R], pq->buffer[largest]) > 0) {
        largest = R;
    }

    // If the largest is not the current index, swap and continue heapifying
    if (largest != index) {
        void *tmp = pq->buffer[index];
        pq->buffer[index] = pq->buffer[largest];
        pq->buffer[largest] = tmp;

        // Recursively heapify the affected subtree
        pqueue_heapify(pq, largest);
    }
}