#define kMaxStrlist 1024  // Define the kMaxStrlist limit
void str_list_add(struct mystr_list* p_list, const struct mystr* p_str, const struct mystr* p_sort_key_str)
{
    // Kill session if string list is over the limit
    if (p_list->list_len >= kMaxStrlist)
    {
        perror("String list exceeded the limit");
        exit(1);
    }

    struct mystr_list_node* p_node;
    /* Expand the node allocation if we have to */
    if (p_list->list_len == p_list->alloc_len)
    {
        if (p_list->alloc_len == 0)
        {
            p_list->alloc_len = 32;
            p_list->p_nodes = vsf_sysutil_malloc(p_list->alloc_len * sizeof(struct mystr_list_node));
        }
        else
        {
            p_list->alloc_len *= 2;
            p_list->p_nodes = vsf_sysutil_realloc(p_list->p_nodes, p_list->alloc_len * sizeof(struct mystr_list_node));
        }
    }
    p_node = &p_list->p_nodes[p_list->list_len];
    p_node->str = s_null_str;
    p_node->sort_key_str = s_null_str;
    str_copy(&p_node->str, p_str);
    if (p_sort_key_str)
    {
        str_copy(&p_node->sort_key_str, p_sort_key_str);
    }
    p_list->list_len++;
}