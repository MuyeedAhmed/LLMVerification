enum cc_stat hashtable_add(HashTable *table, void *key, void *val)
{
    enum cc_stat stat;
    if (table->size >= table->threshold) {
        if ((stat = resize(table, table->capacity << 1)) != CC_OK)
            return stat;
    }

    if (!key) {
        // Handle null key separately
        return add_null_key(table, val);
    }

    const size_t hash = table->hash(key, table->key_len, table->hash_seed);
    const size_t i = hash & (table->capacity - 1);

    TableEntry *replace = table->buckets[i];

    while (replace) {
        if (table->key_cmp(replace->key, key) == 0) {
            replace->value = val;
            return CC_OK;
        }
        replace = replace->next;
    }

    TableEntry *new_entry = table->mem_alloc(sizeof(TableEntry));

    if (!new_entry)
        return CC_ERR_ALLOC;

    new_entry->key = key;
    new_entry->value = val;
    new_entry->hash = hash;
    new_entry->next = table->buckets[i];

    table->buckets[i] = new_entry;
    table->size++;

    return CC_OK;
}

enum cc_stat hashtable_get(HashTable *table, void *key, void **out)
{
    if (!key) {
        // Handle null key separately
        return get_null_key(table, out);
    }

    size_t index = get_table_index(table, key);
    TableEntry *bucket = table->buckets[index];

    while (bucket) {
        if (table->key_cmp(bucket->key, key) == 0) {
            *out = bucket->value;
            return CC_OK;
        }
        bucket = bucket->next;
    }
    return CC_ERR_KEY_NOT_FOUND;
}

enum cc_stat hashtable_remove(HashTable *table, void *key, void **out)
{
    if (!key) {
        // Handle null key separately
        return remove_null_key(table, out);
    }

    const size_t i = get_table_index(table, key);

    TableEntry *e = table->buckets[i];
    TableEntry *prev = NULL;
    TableEntry *next = NULL;

    while (e) {
        next = e->next;

        if (table->key_cmp(key, e->key) == 0) {
            void *value = e->value;

            if (!prev)
                table->buckets[i] = next;
            else
                prev->next = next;

            table->mem_free(e);
            table->size--;
            if (out)
                *out = value;
            return CC_OK;
        }
        prev = e;
        e = next;
    }
    return CC_ERR_KEY_NOT_FOUND;
}