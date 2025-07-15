enum cc_stat cc_deque_iter_remove(CC_DequeIter *iter, void **out)
{
    if (iter->last_removed)
        return CC_ERR_VALUE_NOT_FOUND;

    void *rm;
    enum cc_stat status = cc_deque_remove_at(iter->deque, iter->index - 1, &rm);
    if (status == CC_OK) {
        iter->index--;
        iter->last_removed = true;
        if (out)
            *out = rm;
    }
    return status;
}