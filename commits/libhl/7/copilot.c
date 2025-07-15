void ht_clear(hashtable_t *table) {
    uint32_t i;

    SPIN_LOCK(&table->lock);

    for (i = 0; i < table->size; i++) {
        ht_items_list_t *list = __sync_fetch_and_add(&table->items[i], 0);

        if (!list) {
            continue; // Skip if the list is NULL
        }

        ht_item_t *item = NULL;
        ht_item_t *tmp;

        SPIN_LOCK(&list->lock);

        // Traverse and remove all items in the list
        TAILQ_FOREACH_SAFE(item, &list->head, next, tmp) {
            TAILQ_REMOVE(&list->head, item, next);
            if (table->free_item_cb) {
                table->free_item_cb(item->data);
            }
            free(item->key);
            free(item);
            __sync_sub_and_fetch(&table->count, 1);
        }

        SPIN_UNLOCK(&list->lock);

        // Reset the list for future use
        TAILQ_INIT(&list->head);
    }

    SPIN_UNLOCK(&table->lock);
}