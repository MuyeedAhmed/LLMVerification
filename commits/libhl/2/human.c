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

    // we want to lock the list now to avoid it being destroyed
    // by a concurrent ht_delete() call. 
    // Note that inserting/removing values from the list is thread-safe. 
    // The problem would exist if the list is destroyed while we are 
    // still accessing it so we need to lock it for this purpose

    list_lock(list);

    // we can anyway unlock the table to allow operations which 
    // don't involve the actual linklist
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
            //fprintf(stderr, "Can't create new item: %s", strerror(errno));
            list_unlock(list); 
            return NULL;
        }
        item->hash = hash;
        item->key = strdup(key);
        if (!item->key) {
            //fprintf(stderr, "Can't copy key: %s", strerror(errno));
            list_unlock(list);
            free(item);
            return NULL;
        }
        item->data = data;
        if (push_value(list, item) == 0) {
            count = __sync_add_and_fetch(&table->count, 1);
        } else {
            //fprintf(stderr, "Can't push new value for key: %s", strerror(errno));
            list_unlock(list);
            free(item->key);
            free(item);
            return NULL;
        }
    }

    list_unlock(list);

    if (count > ht_count(table) + HT_GROW_THRESHOLD) {
        ht_grow_table(table);
    }

    if (prev_item) {
        if (table->free_item_cb)
            table->free_item_cb(prev_item->data);
        free(prev_item->key);
        free(prev_item);
    }
    return prev_data;
}

uint32_t ht_count(hashtable_t *table) {
    return __sync_add_and_fetch(&table->count, 0);
}