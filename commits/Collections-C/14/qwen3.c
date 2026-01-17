enum cc_stat cc_list_reduce(CC_List *list, void (*fn)(void *, void *, void *), void *result)
{
    if (!list || !fn || !result)
        return CC_ERR_INVALID_PARAM;

    Node *node = list->head;
    void *acc = result;

    while (node) {
        fn(acc, node->data, acc);
        node = node->next;
    }

    return CC_OK;
}
