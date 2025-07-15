int _ht_set_internal(hashtable_t *table, void *key, size_t klen,
    void *data, size_t dlen, void **prev_data, size_t *prev_len, int copy)
{
uint32_t hash;
void *prev = NULL;
size_t plen = 0;

if (!klen)
    return -1;

PERL_HASH(hash, key, klen);

ht_items_list_t *list = __sync_fetch_and_add(&table->items[hash % __sync_fetch_and_add(&table->size, 0)], 0);

if (__sync_fetch_and_add(&table->growing, 0) || !list) {
    SPIN_LOCK(&table->lock);
    list = __sync_fetch_and_add(&table->items[hash % __sync_fetch_and_add(&table->size, 0)], 0);
    if (!list) {
        list = malloc(sizeof(ht_items_list_t));

#ifdef THREAD_SAFE
#ifdef __MACH__
        list->lock = 0;
#else
        pthread_spin_init(&list->lock, 0);
#endif
#endif
        TAILQ_INIT(&list->head);

        while (!__sync_bool_compare_and_swap(&table->items[hash % __sync_fetch_and_add(&table->size, 0)], NULL, list)) {
            ht_items_list_t *l = __sync_fetch_and_add(&table->items[hash % __sync_fetch_and_add(&table->size, 0)], 0);
            if (l) {
#ifdef THREAD_SAFE
#ifndef __MACH__
                pthread_spin_destroy(&list->lock);
#endif
#endif
                free(list);
                list = l;
                break;
            }
        }
    }

    // Acquire lock on the bucket before releasing the table lock
    SPIN_LOCK(&list->lock);
    SPIN_UNLOCK(&table->lock);
} else {
    // Lock the bucket directly if the table is not growing
    SPIN_LOCK(&list->lock);
}

ht_item_t *item = NULL;
TAILQ_FOREACH(item, &list->head, next) {
    if (((char *)item->key)[0] == ((char *)key)[0] &&
        item->klen == klen &&
        memcmp(item->key, key, klen) == 0)
    {
        prev = item->data;
        plen = item->dlen;
        item->dlen = dlen;
        if (copy) {
            item->data = malloc(dlen);
            memcpy(item->data, data, dlen);
        } else {
            item->data = data;
        }
        break;
    }
}

if (!prev) {
    ht_item_t *item = (ht_item_t *)calloc(1, sizeof(ht_item_t));
    if (!item) {
        SPIN_UNLOCK(&list->lock);
        return -1;
    }
    item->hash = hash;
    item->klen = klen;
    item->key = malloc(klen);
    if (!item->key) {
        SPIN_UNLOCK(&list->lock);
        free(item);
        return -1;
    }
    memcpy(item->key, key, klen);

    if (copy) {
        if (dlen) {
            item->data = malloc(dlen);
            memcpy(item->data, data, dlen);
        } else {
            item->data = NULL;
        }
    } else {
        item->data = data;
    }
    item->dlen = dlen;

    TAILQ_INSERT_TAIL(&list->head, item, next);
    __sync_add_and_fetch(&table->count, 1);
}

SPIN_UNLOCK(&list->lock);

if (ht_count(table) > __sync_fetch_and_add(&table->size, 0) + HT_GROW_THRESHOLD && 
    (!table->max_size || __sync_fetch_and_add(&table->size, 0) < table->max_size))
{
    ht_grow_table(table);
}

if (prev) {
    if (prev_data)
        *prev_data = prev;
    else if (table->free_item_cb)
        table->free_item_cb(prev);
} else if (prev_data) {
    *prev_data = NULL;
}

if (prev_len)
    *prev_len = plen;

return 0;
}