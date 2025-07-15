typedef struct {
    void *value;
    size_t value_len; // Added field to store the length of the value
    int32_t prio;
} pqueue_item_t;

int
pqueue_insert(pqueue_t *pq, int32_t prio, void *value, size_t value_len)
{
    pqueue_item_t *item = malloc(sizeof(pqueue_item_t));
    item->value = malloc(value_len); // Allocate memory for the value
    memcpy(item->value, value, value_len); // Copy the value
    item->value_len = value_len; // Store the length of the value
    item->prio = prio;

    int rc = binheap_insert(pq->heap, (void *)&item->prio, sizeof(item->prio), item, sizeof(pqueue_item_t));

    if (binheap_count(pq->heap) > pq->max_size)
        pqueue_drop_items(pq, binheap_count(pq->heap) - pq->max_size);

    return rc;
}

int
pqueue_pull_highest(pqueue_t *pq, void **value, int32_t *prio, size_t *value_len)
{
    pqueue_item_t *item = NULL;

    int rc = (pq->mode == PQUEUE_MODE_HIGHEST)
           ? binheap_delete_maximum(pq->heap, (void **)&item, NULL)
           : binheap_delete_minimum(pq->heap, (void **)&item, NULL);

    if (rc == 0) {
        if (value) {
            *value = item->value;
            if (value_len) {
                *value_len = item->value_len; // Return the length of the value
            }
        } else if (pq->free_value_cb) {
            pq->free_value_cb(item->value);
        }

        if (prio)
            *prio = item->prio;

        free(item);
    }
    return rc;
}

int
pqueue_pull_lowest(pqueue_t *pq, void **value, int32_t *prio, size_t *value_len)
{
    pqueue_item_t *item = NULL;

    int rc = (pq->mode == PQUEUE_MODE_HIGHEST)
           ? binheap_delete_minimum(pq->heap, (void **)&item, NULL)
           : binheap_delete_maximum(pq->heap, (void **)&item, NULL);

    if (rc == 0) {
        if (value) {
            *value = item->value;
            if (value_len) {
                *value_len = item->value_len; // Return the length of the value
            }
        } else if (pq->free_value_cb) {
            pq->free_value_cb(item->value);
        }

        if (prio)
            *prio = item->prio;

        free(item);
    }
    return rc;
}