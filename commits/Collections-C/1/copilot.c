/**
 * Removes a NULL key mapping from the specified hash table and returns the
 * value that was mapped to the NULL key. In case the NULL key doesn't exist,
 * NULL is returned. NULL might also be returned if a NULL key is mapped to a
 * NULL value.
 *
 * @param[in] table the table from which the NULL key mapping is being removed
 * @return the value associated with the NULL key, or NULL if the NULL key was not mapped
 */
static void *remove_null_key(HashTable *table)
{
    TableEntry *entry = table->buckets[0];
    TableEntry *prev = NULL;

    while (entry) {
        if (entry->key == NULL) {
            void *value = entry->value;

            if (prev == NULL) {
                // The NULL key is the first entry in the bucket
                table->buckets[0] = entry->next;
            } else {
                // The NULL key is not the first entry
                prev->next = entry->next;
            }

            free(entry);
            table->size--;
            return value;
        }

        prev = entry;
        entry = entry->next;
    }

    // NULL key not found
    return NULL;
}