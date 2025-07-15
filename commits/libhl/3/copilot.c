void *ht_pop(hashtable_t *table, char *key) {
    uint32_t hash;
    void *prev_data = NULL;

    PERL_HASH(hash, key, strlen(key));
    MUTEX_LOCK(&table->lock);
    linked_list_t *list = table->items[hash % table->size];

    if (!list) {
        MUTEX_UNLOCK(&table->lock);
        return NULL;
    }

    list_lock(list);
    MUTEX_UNLOCK(&table->lock);

    ht_iterator_arg_t arg = {
        .index = UINT_MAX,
        .set = false,
        .item = {
            .hash = hash,
            .key = key,
            .data = NULL
        }
    };
    foreach_list_value(list, _get_item, &arg);

    if (arg.index != UINT_MAX) {
        prev_data = arg.item.data;
        ht_item_t *item = (ht_item_t *)fetch_value(list, arg.index);
        if (item) {
            free(item->key);
            free(item);

            // Atomically decrement the count
            __sync_sub_and_fetch(&table->count, 1);
        }
    }

    list_unlock(list);

    return prev_data;
}