enum cc_stat treetable_iter_remove(TreeTableIter *iter, void **out)
{
    if (!iter->current)
        return CC_ERR_KEY_NOT_FOUND;

    if (out)
        *out = iter->current->value;

    remove_node(iter->table, iter->current);
    iter->current = NULL;

    return CC_OK;
}