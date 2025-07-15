/**
 * Filters the Stack by creating a new Stack that contains all elements from the
 * original Stack that return true on pred(element) without modifying the original
 * Stack.
 *
 * @param[in] stack   Stack that is to be filtered
 * @param[in] predicate predicate function which returns true if the element should
 *                 be kept in the filtered stack
 * @param[out] out pointer to where the new filtered Stack is to be stored
 *
 * @return CC_OK if the Stack was filtered successfully, CC_ERR_OUT_OF_RANGE
 * if the Stack is empty, or CC_ERR_ALLOC if the memory allocation for the
 * new Stack failed.
 */
enum cc_stat stack_filter(Stack *stack, bool (*predicate)(const void *), Stack **out)
{
    if (stack_size(stack) == 0) {
        // Return an error if the stack is empty
        return CC_ERR_OUT_OF_RANGE;
    }

    // Create a new stack for the filtered elements
    enum cc_stat status = stack_new(out);
    if (status != CC_OK) {
        return CC_ERR_ALLOC;
    }

    StackIter iter;
    stack_iter_init(&iter, stack);

    void *element;
    while (stack_iter_next(&iter, &element) == CC_OK) {
        if (predicate(element)) {
            // Add the element to the new stack if it satisfies the predicate
            status = stack_push(*out, element);
            if (status != CC_OK) {
                stack_destroy(*out);
                return CC_ERR_ALLOC;
            }
        }
    }

    return CC_OK;
}