void *ht_set(hashtable_t *table, char *key, void *data) {
    uint32_t hash;
    void *prev_data = NULL;

    PERL_HASH(hash, key, strlen(key));
    MUTEX_LOCK(&table->lock);
    linked_list_t *list = table->items[hash % table->size];
    if (!list) {
        list = create_list();
        set_free_value_callback(list, free);
        table->items[hash % table->size] = list;
    }

    list_lock(list);
    MUTEX_UNLOCK(&table->lock);

    ht_iterator_arg_t arg = {
        .index = UINT_MAX,
        .set   = true,
        .item  = {
            .hash  = hash,
            .key   = key,
            .data  = data,
        }
    };

    if (list_count(list))
        foreach_list_value(list, _get_item, &arg);

    if (arg.index != UINT_MAX) {
        prev_data = arg.item.data;
    } else {
        ht_item_t *item = (ht_item_t *)calloc(1, sizeof(ht_item_t));
        if (!item) {
            list_unlock(list);
            return NULL;
        }
        item->hash = hash;
        item->key = strdup(key);
        if (!item->key) {
            list_unlock(list);
            free(item);
            return NULL;
        }
        item->data = data;
        if (push_value(list, item) == 0) {
            MUTEX_LOCK(&table->lock); // Lock before modifying table->count
            table->count++;
            MUTEX_UNLOCK(&table->lock);
        } else {
            list_unlock(list);
            free(item->key);
            free(item);
            return NULL;
        }
    }

    list_unlock(list);

    MUTEX_LOCK(&table->lock); // Lock before checking table->count
    if (table->count > table->size + HT_GROW_THRESHOLD) {
        ht_grow_table(table);
    }
    MUTEX_UNLOCK(&table->lock);

    return prev_data;
}

uint32_t ht_count(hashtable_t *table) {
    uint32_t count;
    MUTEX_LOCK(&table->lock); // Lock before accessing table->count
    count = table->count;
    MUTEX_UNLOCK(&table->lock);
    return count;
}