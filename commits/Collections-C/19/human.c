static INLINE RBNode *tree_min(CC_TreeTable const * const table, RBNode *n)
{
    RBNode *s = table->sentinel;
    
    if (n == s)
        return s;

    while (n->left != s)
        n = n->left;
    return n;
}

static INLINE RBNode *tree_max(CC_TreeTable const * const table, RBNode *n)
{
    RBNode *s = table->sentinel;
    
    if (n == s)
        return s;

    while (n->right != s)
        n = n->right;
    return n;
}