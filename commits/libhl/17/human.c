inline int queue_push_position(queue_t *q, uint32_t pos, void *value)
{
    queue_entry_t *entry = create_entry(q->refcnt);
    if (!entry)
        return -1;

    entry->value = value;

    queue_entry_t *prev = pos == 0 ? ATOMIC_READ(q->head) : queue_entry_at_pos(q, pos-1);
    if (!prev)
        return -1;

    retain_ref(q->refcnt, q->head->node);

    queue_entry_t *next = get_node_ptr(deref_link(prev->refcnt, &prev->next));

    while (1) {
        if (ATOMIC_READ(prev->next) != next->node) {
            release_ref(next->refcnt, next->node);
            next = get_node_ptr(deref_link(prev->refcnt, &prev->next));
            continue;
        }
        entry->prev = prev->node;
        entry->next = next->node;
        if (ATOMIC_CMPXCHG(prev->next, next->node, entry->node)) {
            retain_ref(entry->refcnt, entry->node);
            break;
        }
    }
    ATOMIC_INCREMENT(q->length, 1);
    push_common(entry, next);
    return 0;

}