void *ht_set(hashtable_t *table, char *key, void *data) {
    uint32_t hash;
    uint32_t count = 0;
    void *prev_data = NULL;
    ht_item_t *prev_item = NULL;

    PERL_HASH(hash, key, strlen(key));
    MUTEX_LOCK(&table->lock);
    linked_list_t *list = table->items[hash%table->size];
    if (!list) {
        list = create_list();
        set_free_value_callback(list, free);
        table->items[hash%table->size] = list;
    }

    list_lock(list);

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
        prev_item = (ht_item_t *)fetch_value(list, arg.index);
        if (prev_item) {
            if (table->free_item_cb)
                table->free_item_cb(prev_item->data);
            free(prev_item->key);
            free(prev_item);
            uint32_t new_count = __sync_sub_and_fetch(&table->count, 1);
            (void)new_count; // Prevent unused variable warning
        }
    } else {
        ht_item_t *item = (ht_item_t *)calloc(1, sizeof(ht_item_t));
        if (!item) {
            list_unlock(list);
            MUTEX_UNLOCK(&table->lock);
            return NULL;
        }
        item->hash = hash;
        item->key = strdup(key);
        if (!item->key) {
            list_unlock(list);
            MUTEX_UNLOCK(&table->lock);
            free(item);
            return NULL;
        }
        item->data = data;
        if (push_value(list, item) == 0) {
            uint32_t new_count = __sync_add_and_fetch(&table->count, 1);
            (void)new_count; // Prevent unused variable warning
        } else {
            list_unlock(list);
            MUTEX_UNLOCK(&table->lock);
            free(item->key);
            free(item);
            return NULL;
        }
    }

    list_unlock(list);
    MUTEX_UNLOCK(&table->lock);

    if (__sync_add_and_fetch(&table->count, 0) > table->size + HT_GROW_THRESHOLD) {
        ht_grow_table(table);
    }

    return prev_data;
}

uint32_t ht_count(hashtable_t *table) {
    return __sync_add_and_fetch(&table->count, 0);
}
