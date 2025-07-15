# Code Citations

## License: unknown
https://github.com/weipengyiyu/Data_structure/tree/791e2b652d3fb3dee629c97257f8f4b60a6a55a1/test_C/hashtable.c

```
new HashTable based on the specified HashTableConf struct and returns
 * a status code.
 *
 * The table is allocated using the memory allocators specified in the HashTableConf
 * struct.
 *
 * @param[in] conf the HashTable conf structure
 * @param[out] out Pointer to where the
```


## License: GPL_3_0
https://github.com/neobreaker/YH/tree/4c7a03f56390973f72249dec8a531f9977f4d5da/src/collect/hashtable.c

```
conf->mem_calloc(1, sizeof(HashTable));

    if (!table)
        return CC_ERR_ALLOC;

    table->capacity = round_pow_two(conf->initial_capacity);
    table->buckets = conf->mem_calloc(table->capacity, sizeof(TableEntry*))
```


## License: unknown
https://github.com/xitren/XitLibs/tree/650d583e5a84df6c2b500d9f84d6d5dcd66bb355/src/models/hashtable.c

```
allocation for the
 * new buffer failed.
 */
static enum cc_stat resize(HashTable *t, size_t new_capacity)
{
    if (t->capacity == MAX_POW_TWO)
        return CC_ERR_MAX_CAPACITY;

    TableEntry **new_buckets = t->mem_calloc(new_capacity, sizeof(TableEntry*));
```

