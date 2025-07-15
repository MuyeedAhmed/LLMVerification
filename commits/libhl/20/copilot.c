int queue_push_right(queue_t *q, void *value)
{
    queue_entry_t *entry = create_entry(q->refcnt);
    if (!entry)
        return -1;

    entry->value = value;
    queue_entry_t *next = ATOMIC_READ(q->tail);
    retain_ref(next->refcnt, ATOMIC_READ(next->node)); // Retain the tail node
    queue_entry_t *prev = get_node_ptr(deref_link(next->refcnt, &next->prev));
    if (!prev) {
        // Fixed memory leak: release the retained tail node before returning
        release_ref(next->refcnt, ATOMIC_READ(next->node));
        destroy_entry(entry); // Free the newly created entry
        return -1;
    }

    while (1) {
        if (ATOMIC_READ(prev->next) != ATOMIC_READ(next->node)) {
            prev = help_insert(prev, next);
            continue;
        }
        entry->prev = ATOMIC_READ(prev->node);
        entry->next = ATOMIC_READ(next->node);
        if (ATOMIC_CMPXCHG(prev->next, ATOMIC_READ(next->node), ATOMIC_READ(entry->node))) {
            retain_ref(entry->refcnt, ATOMIC_READ(entry->node));
            break;
        }
    }

    ATOMIC_INCREMENT(q->length, 1);
    push_common(entry, next);

    // Release the retained tail node after successful insertion
    release_ref(next->refcnt, ATOMIC_READ(next->node));

    return 0;
}