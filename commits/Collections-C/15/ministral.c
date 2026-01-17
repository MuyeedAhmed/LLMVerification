enum cc_stat stack_filter(Stack *stack, bool (*predicate)(const void *), Stack **out) {
    if (!stack || !out) {
        return CC_ERR_ALLOC;
    }

    if (stack_size(stack) == 0) {
        return CC_ERR_OUT_OF_RANGE;
    }

    Stack *filtered_stack;
    if (stack_new(&filtered_stack) != CC_OK) {
        return CC_ERR_ALLOC;
    }

    StackIter iter;
    stack_iter_init(&iter, stack);

    void *element;
    while (stack_iter_next(&iter, &element) == CC_OK) {
        if (predicate(element)) {
            if (stack_push(filtered_stack, element) != CC_OK) {
                stack_destroy(filtered_stack);
                return CC_ERR_ALLOC;
            }
        }
    }

    *out = filtered_stack;
    return CC_OK;
}
