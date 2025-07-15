/**
 * Pushes the element into the priority queue.
 *
 * @param[in] pq the priority queue in which the element is to be pushed
 * @param[in] element the element to be pushed
 *
 * @return CC_OK if the element was successfully pushed, or CC_ERR_ALLOC
 * if the memory allocation for the new element failed.
 */
enum cc_stat cc_pqueue_push(CC_PQueue *pq, void *element)
{
    if (!pq || !element) {
        return CC_ERR_INVALID_ARGUMENT;
    }

    // Check if the current size exceeds or equals the capacity
    if (pq->size >= pq->capacity) {
        enum cc_stat status = expand_capacity(pq);
        if (status != CC_OK) {
            return status; // Return error if capacity expansion fails
        }
    }

    // Insert the new element at the end of the buffer
    size_t i = pq->size;
    pq->buffer[i] = element;
    pq->size++;

    // Restore the heap property by moving the element up
    while (i > 0) {
        size_t parent = CC_PARENT(i);
        if (pq->cmp(pq->buffer[i], pq->buffer[parent]) <= 0) {
            break; // Stop if the heap property is satisfied
        }

        // Swap the current element with its parent
        void *tmp = pq->buffer[i];
        pq->buffer[i] = pq->buffer[parent];
        pq->buffer[parent] = tmp;

        i = parent; // Move up to the parent's index
    }

    return CC_OK;
}