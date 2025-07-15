enum cc_stat array_get_last(Array *ar, void **out)
{
    if (ar->size == 0)
        return CC_ERR_OUT_OF_RANGE;

    return array_get_at(ar, ar->size - 1, out);
}