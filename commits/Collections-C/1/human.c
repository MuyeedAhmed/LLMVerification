void *remove_null_key(HashTable *table)
{
    TableEntry *e = table->buckets[0];

    TableEntry *prev = NULL;
    TableEntry *next = NULL;
 
    while (e) {
        next = e->next;
        
        if (e->key == NULL) {
            void *value = e->value;

            if (!prev)
                table->buckets[0] = next;
            else
                prev->next = next;

            free(e);
            table->size--;
            return value;
        }
        prev = e;
        e = next;
    }
    return NULL;
}