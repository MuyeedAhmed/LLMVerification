enum cc_stat hashtable_new_conf(HashTableConf const * const conf, HashTable **out)
{
    HashTable *table = conf->mem_calloc(1, sizeof(HashTable));

    if (!table)
        return CC_ERR_ALLOC;

    table->capacity = round_pow_two(conf->initial_capacity);
    table->buckets  = conf->mem_calloc(table->capacity, sizeof(TableEntry *));

    if (!table->buckets) {
        conf->mem_free(table);
        return CC_ERR_ALLOC;
    }

    table->hash        = conf->hash;
    table->key_cmp     = conf->key_compare;
    table->load_factor = conf->load_factor;
    table->hash_seed   = conf->hash_seed;
    table->key_len     = conf->key_length;
    table->size        = 0;
    table->mem_alloc   = conf->mem_alloc;
    table->mem_calloc  = conf->mem_calloc;
    table->mem_free    = conf->mem_free;
    table->threshold   = table->capacity * table->load_factor;

    *out = table;
    return CC_OK;
}

static enum cc_stat resize(HashTable *t, size_t new_capacity)
{
    if (t->capacity == MAX_POW_TWO)
        return CC_ERR_MAX_CAPACITY;

    TableEntry **new_buckets = t->mem_calloc(new_capacity, sizeof(TableEntry *));

    if (!new_buckets)
        return CC_ERR_ALLOC;

    TableEntry **old_buckets = t->buckets;

    move_entries(old_buckets, new_buckets, t->capacity, new_capacity);

    t->buckets   = new_buckets;
    t->capacity  = new_capacity;
    t->threshold = t->load_factor * new_capacity;

    t->mem_free(old_buckets);

    return CC_OK;
}
