int _ht_set_internal(hashtable_t *table, void *key, size_t klen,
    void *data, size_t dlen, void **prev_data, size_t *prev_len, int copy)
{
uint32_t hash;
void *prev = NULL;
size_t plen = 0;

if (!klen)
    return -1;

PERL_HASH(hash, key, klen);

ht_items_list_t *list = ATOMIC_READ(table->items[hash%ATOMIC_READ(table->size)]);

if (ATOMIC_READ(table->growing) || !list) {
    MUTEX_LOCK(table->lock);
    list = ATOMIC_READ(table->items[hash%ATOMIC_READ(table->size)]);
    if (!list) {
        list = malloc(sizeof(ht_items_list_t));

#ifdef THREAD_SAFE
        SPIN_INIT(list->lock);
#endif
        TAILQ_INIT(&list->head);

        while (!__sync_bool_compare_and_swap(&table->items[hash%ATOMIC_READ(table->size)], NULL, list)) {
            ht_items_list_t *l = ATOMIC_READ(table->items[hash%ATOMIC_READ(table->size)]);
            if (l) {
#ifdef THREAD_SAFE
                SPIN_DESTROY(list->lock);
#endif
                free(list);
                list = l;
                break;
            }
        }
    }
    SPIN_LOCK(list->lock);
    MUTEX_UNLOCK(table->lock);
} else {
    SPIN_LOCK(list->lock);
}

ht_item_t *item = NULL;
TAILQ_FOREACH(item, &list->head, next) {
    if (/*ht_item->hash == arg->item.hash && */
        ((char *)item->key)[0] == ((char *)key)[0] &&
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
        //fprintf(stderr, "Can't create new item: %s\n", strerror(errno));
        SPIN_UNLOCK(list->lock);
        return -1;
    }
    item->hash = hash;
    item->klen = klen;
    item->key = malloc(klen);
    if (!item->key) {
        //fprintf(stderr, "Can't copy key: %s\n", strerror(errno));
        SPIN_UNLOCK(list->lock);
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
    ATOMIC_INCREMENT(table->count);
}

SPIN_UNLOCK(list->lock);

if (ht_count(table) > ATOMIC_READ(table->size) + HT_GROW_THRESHOLD && 
    (!table->max_size || ATOMIC_READ(table->size) < table->max_size) &&
    !ATOMIC_READ(table->growing))
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