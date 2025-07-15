int ht_exists(hashtable_t *table, void *key, size_t klen) {
    uint32_t hash;

    // Validate input parameters
    if (!table || !key || klen == 0) {
        return -1; // Error: Invalid parameters
    }

    // Compute the hash for the given key
    PERL_HASH(hash, key, klen);

    // Fetch the list corresponding to the hash
    ht_items_list_t *list = __sync_fetch_and_add(&table->items[hash % table->size], 0);

    if (!list) {
        return 0; // Key does not exist
    }

    // Lock the list to ensure thread safety
    SPIN_LOCK(&list->lock);

    ht_item_t *item = NULL;

    // Iterate through the list to check for the key
    TAILQ_FOREACH(item, &list->head, next) {
        if (((char *)item->key)[0] == ((char *)key)[0] &&
            item->klen == klen &&
            memcmp(item->key, key, klen) == 0) {
            SPIN_UNLOCK(&list->lock);
            return 1; // Key exists
        }
    }

    // Unlock the list after iteration
    SPIN_UNLOCK(&list->lock);

    return 0; // Key does not exist
}