enum cc_stat cc_list_reduce(CC_List *list, void (*fn)(void *, void *, void *), void *result)
{
    size_t list_size = cc_list_size(list);

    if (list_size == 0)
        return CC_ERR_OUT_OF_RANGE;

    if (list_size == 1)
    {
        fn(list->head->data, NULL, result);
        return CC_OK;
    }

    fn(list->head->data, list->head->next->data, result);

    // the current start will be the second element of list
    Node *curr = list->head->next->next;
    Node *next = NULL;

    while (curr)
    {
        next = curr->next;

        fn(result, curr->data, result);

        curr = next;
    }

    return CC_OK;
}