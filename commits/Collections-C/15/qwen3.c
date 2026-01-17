enum cc_stat stack_filter(Stack *stack, bool (*predicate)(const void *), Stack **out)
{
    if (!stack || !predicate || !out)
        return CC_ERR_INVALID_PARAM;

    if (stack_size(stack) == 0) {
        *out = NULL;
        return CC_ERR_OUT_OF_RANGE;
    }

    Stack *new_stack;
    enum cc_stat status = stack_new(&new_stack);
    if (status != CC_OK) {
        return status;
    }

    StackIter iter;
    stack_iter_init(&iter, stack);

    void *element;
    while (stack_iter_next(&iter, &element) == CC_OK) {
        if (predicate(element)) {
            status = stack_push(new_stack, element);
            if (status != CC_OK) {
                stack_destroy(new_stack);
                return status;
            }
        }
    }

    *out = new_stack;
    return CC_OK;
}
