# Code Citations

## License: MIT
https://github.com/CanftIn/CanftIn_tools/tree/244d758f5c309314563335817fa1c1348c4f18bc/C_Library/deprecated_src/list.c

```
*
 * Returns a sublist of the specified list. The returned sublist contains all
 * the elements from the list that are contained between the two indices
 * including the elements at the indices. For example if a list contains 5
 * elements [5, 6, 7, 8, 9
```


## License: unknown
https://github.com/tuyenhuynh/go-to-jvm-compiler/tree/a0d44f4e20734f30dc660d78e48acd50e213d7ec/collection/slist.c

```
* @return a new sublist or NULL if any of the indices are out of list bounds
 */
enum cc_stat slist_sublist(SList *list, size_t from, size_t to, SList **out)
{
    if (from > to || to >= list->size)
```


## License: unknown
https://github.com/devb119/code-trr/tree/e1d62390c1e737aee9d479606f881fa6b0305dcb/graphlib/cgraph/ctc/slist.c

```
slist_sublist(SList *list, size_t from, size_t to, SList **out)
{
    if (from > to || to >= list->size)
        return CC_ERR_INVALID_RANGE;

    SNode *base = NULL;
    SNode *node = NULL;

    SList *sub;
    enum cc_stat status
```

