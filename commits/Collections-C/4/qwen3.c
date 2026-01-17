enum cc_stat slist_sublist(SList *list, size_t from, size_t to, SList **out)
{
    if (from > to || to >= list->size)
        return CC_ERR_INVALID_RANGE;

    SNode *base = NULL;
    SNode *node = NULL;

    SList *sub;
    enum cc_stat status = slist_new(&sub);

    if (status != CC_OK)
        return status;

    status = get_node_at(list, from, &node, &base);

    if (status != CC_OK) {
        slist_destroy(sub);
        return status;
    }

    size_t i;
    for (i = from; i <= to; i++) {
        status = slist_add(sub, node->data);
        if (status != CC_OK) {
            slist_destroy(sub);
            return status;
        }
        node = node->next;
    }
    *out = sub;
    return CC_OK;
}
