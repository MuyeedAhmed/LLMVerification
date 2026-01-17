int ht_exists(hashtable_t *table, void *key, size_t klen) {
    uint32_t hash;
    ht_item_t *item = NULL;

    if (!table || !key)
        return -1;

    PERL_HASH(hash, key, klen);
    ht_items_list_t *list = __sync_fetch_and_add(&table->items[hash%table->size], 0);

    if (!list)
        return 0;

    SPIN_LOCK(&list->lock);

    TAILQ_FOREACH(item, &list->head, next) {
        if (((char *)item->key)[0] == ((char *)key)[0] &&
            item->klen == klen &&
            memcmp(item->key, key, klen) == 0) {
            SPIN_UNLOCK(&list->lock);
            return 1;
        }
    }

    SPIN_UNLOCK(&list->lock);
    return 0;
}
