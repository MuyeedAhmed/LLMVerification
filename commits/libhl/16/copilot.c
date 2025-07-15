static inline list_entry_t *pick_entry(linked_list_t *list, uint32_t pos) 
{
    unsigned int i;
    list_entry_t *entry = NULL;

    if (list->length <= pos)
        return NULL;

    uint32_t half_length = list->length >> 1;

    MUTEX_LOCK(&list->lock); // Lock the list before accessing its entries

    if (list->cur && (uint32_t)abs((int)(list->pos - pos)) < half_length) {
        entry = list->cur;
        if (list->pos != pos) {
            if (list->pos < pos) {
                for (i = list->pos; i < pos; i++) {
                    if (!entry->next) {
                        entry = NULL; // Handle potential race condition
                        break;
                    }
                    entry = entry->next;
                }
            } else if (list->pos > pos) {
                for (i = list->pos; i > pos; i--) {
                    if (!entry->prev) {
                        entry = NULL; // Handle potential race condition
                        break;
                    }
                    entry = entry->prev;
                }
            }
        }
    } else {
        if (pos > half_length) {
            entry = list->tail;
            for (i = list->length - 1; i > pos; i--) {
                if (!entry->prev) {
                    entry = NULL; // Handle potential race condition
                    break;
                }
                entry = entry->prev;
            }
        } else {
            entry = list->head;
            for (i = 0; i < pos; i++) {
                if (!entry->next) {
                    entry = NULL; // Handle potential race condition
                    break;
                }
                entry = entry->next;
            }
        }
    }

    if (entry) {
        list->pos = pos;
        list->cur = entry;
    }

    MUTEX_UNLOCK(&list->lock); // Unlock the list after accessing its entries

    return entry;
}