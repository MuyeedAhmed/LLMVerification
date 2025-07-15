void
ht_grow_table(hashtable_t *table)
{
    // Lock the table globally to prevent any operation on the actual one
    MUTEX_LOCK(table->lock);

    if (table->max_size && ATOMIC_READ(table->size) >= table->max_size) {
        MUTEX_UNLOCK(table->lock);
        return;
    }

    uint32_t i;
    uint32_t new_size = ATOMIC_READ(table->size) << 1;

    if (table->max_size && new_size > table->max_size)
        new_size = table->max_size;

    ht_items_list_t **new_items_list = 
        (ht_items_list_t **)calloc(new_size, sizeof(ht_items_list_t *));

    if (!new_items_list) {
        MUTEX_UNLOCK(table->lock);
        return;
    }

    ht_items_list_t **old_items_list = table->items; // Store the old items array
    ht_item_t *item = NULL;

    for (i = 0; i < ATOMIC_READ(table->size); i++) {
        ATOMIC_INCREMENT(table->growing);

        ht_items_list_t *list = NULL;
        do {
            list = ATOMIC_READ(old_items_list[i]);
        } while (!__sync_bool_compare_and_swap(&old_items_list[i], list, NULL));

        if (!list)
            continue;

        SPIN_LOCK(list->lock);
        while ((item = TAILQ_FIRST(&list->head))) {
            TAILQ_REMOVE(&list->head, item, next);
            ht_items_list_t *new_list = new_items_list[item->hash % new_size];
            if (!new_list) {
                new_list = malloc(sizeof(ht_items_list_t));
                TAILQ_INIT(&new_list->head);
                SPIN_INIT(new_list->lock);
                new_items_list[item->hash % new_size] = new_list;
            }
            TAILQ_INSERT_TAIL(&new_list->head, item, next);
        }
        SPIN_UNLOCK(list->lock);
        SPIN_DESTROY(list->lock);
        free(list);
    }

    ATOMIC_SET(table->items, new_items_list);
    ATOMIC_SET(table->size, new_size);
    ATOMIC_SET(table->growing, 0);

    // Free the old items array
    free(old_items_list);

    MUTEX_UNLOCK(table->lock);
}