static INLINE RBNode *tree_min(CC_TreeTable const * const table, RBNode *n)
{
    if (n == table->sentinel) {
        // If the input node is the sentinel, return it immediately
        return table->sentinel;
    }

    while (n->left != table->sentinel) {
        n = n->left;
    }
    return n;
}

static INLINE RBNode *tree_max(CC_TreeTable const * const table, RBNode *n)
{
    if (n == table->sentinel) {
        // If the input node is the sentinel, return it immediately
        return table->sentinel;
    }

    while (n->right != table->sentinel) {
        n = n->right;
    }
    return n;
}