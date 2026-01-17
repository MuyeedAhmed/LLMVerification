static INLINE RBNode *tree_min(CC_TreeTable const * const table, RBNode *n)
{
    RBNode *s = table->sentinel;
    while (n != s && n->left != s)
        n = n->left;
    return n;
}

static INLINE RBNode *tree_max(CC_TreeTable const * const table, RBNode *n)
{
    RBNode *s = table->sentinel;
    while (n != s && n->right != s)
        n = n->right;
    return n;
}
