/**
 * Reduces the list to a single value by applying the specified function
 * to each element in the list and accumulating the result.
 *
 * @param[in] list the list on which this operation is performed
 * @param[in] fn the operation function that is to be invoked on each list element
 *               and the accumulated result. The function should take three
 *               arguments: the current result, the current element, and a pointer
 *               to the result.
 * @param[in, out] result the pointer which will collect the end result
 *
 * @return CC_OK if the operation was successful, or CC_ERR_OUT_OF_RANGE if the
 * list is empty.
 */
enum cc_stat cc_list_reduce(CC_List *list, void (*fn)(void *, void *, void *), void *result)
{
    if (list->size == 0) {
        // Return an error if the list is empty
        return CC_ERR_OUT_OF_RANGE;
    }

    Node *current = list->head;

    while (current) {
        // Apply the function to the current result and the current element
        fn(result, current->data, result);
        current = current->next;
    }

    return CC_OK;
}