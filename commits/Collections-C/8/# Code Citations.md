# Code Citations

## License: unknown
https://github.com/weipengyiyu/Data_structure/tree/791e2b652d3fb3dee629c97257f8f4b60a6a55a1/test_C/hashtable.c

```
*table, void *key, void *val)
{
    enum cc_stat stat;
    if (table->size >= table->threshold) {
        if ((stat = resize(table, table->capacity << 1)) != CC_OK)
            return stat;
```


## License: unknown
https://github.com/IvanHod/scala-compiler/tree/10ec10dd3ffa0c91b7fb887bb34088465b67ee83/collection/hashtable.c

```
, table->key_len, table->hash_seed);
    const size_t i = hash & (table->capacity - 1);

    TableEntry *replace = table->buckets[i];

    while (replace) {
        if (table->key_cmp(replace->key
```


## License: GPL_3_0
https://github.com/still-dreaming-1/vim-project-tags/tree/562732df2256b52304a15405eb5a566bf2df773a/static%20test%20data/Collections-C/src/hashtable.c

```
, table->hash_seed);
    const size_t i = hash & (table->capacity - 1);

    TableEntry *replace = table->buckets[i];

    while (replace) {
        if (table->key_cmp(replace->key, key) ==
```

