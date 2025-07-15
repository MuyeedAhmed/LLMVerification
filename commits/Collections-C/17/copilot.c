/**
 * Removes the last returned element by <code>cc_deque_iter_next()</code>
 * function without invalidating the iterator and optionally sets the out
 * parameter to the value of the removed element.
 *
 * @note This function should only ever be called after a call to <code>
 * cc_deque_iter_next()</code>
 *
 * @param[out] out Pointer to where the removed element is stored, or NULL
 *                 if it is to be ignored
 * @param[in] iter the iterator on which this operation is being performed
 *
 * @return CC_OK if the element was successfully removed, CC_ERR_OUT_OF_RANGE
 * if the iterator state is invalid, or CC_ERR_VALUE_NOT_FOUND if the value
 * was already removed.
 */
enum cc_stat cc_deque_iter_remove(CC_DequeIter *iter, void **out)
{
    if (iter->last_removed) {
        // Prevent removing the same element twice
        return CC_ERR_VALUE_NOT_FOUND;
    }

    if (iter->index == 0 || iter->index > iter->deque->size) {
        // Ensure the iterator is in a valid state
        return CC_ERR_OUT_OF_RANGE;
    }

    // Decrement the index to point to the last returned element
    iter->index--;

    // Remove the element at the current index
    enum cc_stat status = cc_deque_remove_at(iter->deque, iter->index, out);
    if (status == CC_OK) {
        iter->last_removed = true; // Mark the last element as removed
    }

    return status;
}