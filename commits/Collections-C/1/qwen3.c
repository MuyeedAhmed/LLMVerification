void *remove_null_key(HashTable *table)
{
    TableEntry *entry = table->buckets[0];
    TableEntry *prev  = NULL;
    TableEntry *next;

    while (entry) {
        next = entry->next;

        if (entry->key == NULL) {
            void *value = entry->value;

            if (!prev)
                table->buckets[0] = next;
            else
                prev->next = next;

            free(entry);
            table->size--;
            return value;
        }
        prev  = entry;
        entry = next;
    }
    return NULL;
}
