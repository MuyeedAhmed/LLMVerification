void ht_clear(hashtable_t *table) {
    uint32_t i;
    SPIN_LOCK(&table->lock);
    for (i = 0; i < table->size && __sync_fetch_and_add(&table->count, 0) > 0; i++)
    {
        ht_item_t *item = NULL;
        ht_item_t *tmp;

        ht_items_list_t *list = __sync_fetch_and_add(&table->items[i], 0);

        if (!list)
            continue;

        TAILQ_FOREACH_SAFE(item, &list->head, next, tmp) {
            TAILQ_REMOVE(&list->head, item, next);
            if (table->free_item_cb)
                table->free_item_cb(item->data);
            free(item->key);
            free(item);
            __sync_sub_and_fetch(&table->count, 1);
        }

        __sync_bool_compare_and_swap(&table->items[i], list, NULL);
#ifdef THREAD_SAFE
#ifndef __MACH__
        pthread_spin_destroy(&list->lock);
#endif
#endif
        free(list);
    }
    SPIN_UNLOCK(&table->lock);
}