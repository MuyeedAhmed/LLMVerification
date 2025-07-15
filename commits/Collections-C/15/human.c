enum cc_stat stack_filter(Stack *stack, bool (*predicate)(const void *), Stack **out)
{
    if (stack_size(stack) == 0)
        return CC_ERR_OUT_OF_RANGE;

    StackIter iter;
    stack_iter_init(&iter, stack);

    stack_new(out);

    void *e;
    while (stack_iter_next(&iter, &e) != CC_ITER_END)
    {
        if (predicate(e))
            stack_push(*out, e);
    }

    return CC_OK;
}