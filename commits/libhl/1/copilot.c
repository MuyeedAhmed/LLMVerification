static inline list_entry_t *shift_entry(linked_list_t *list) 
{
    list_entry_t *entry;
    MUTEX_LOCK(&list->lock);
    entry = list->head;
    if (entry) 
    {
        list->head = entry->next;
        if (list->head) 
            list->head->prev = NULL;
        list->length--;
        entry->list = NULL;

        // Update cursor position if the removed entry is before the cursor
        if (list->cur && list->cur != entry) 
        {
            list->pos--;
        }
        else if (list->cur == entry) 
        {
            list->cur = NULL;
            list->pos = 0;
        }
    }
    if (list->length == 0)
        list->head = list->tail = NULL;
    MUTEX_UNLOCK(&list->lock);
    return entry;
}