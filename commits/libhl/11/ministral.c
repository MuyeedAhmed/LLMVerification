static void
binomial_tree_node_increase_key(binomial_tree_node_t *node, int incr)
{
    void *okey = node->key;
    size_t oklen = node->klen;
    void *nkey = NULL;
    size_t nklen = 0;

    if (incr == 0)
        return;

    node->bh->cbs->incr(okey, oklen, &nkey, &nklen, incr);

    binomial_tree_node_t *parent = node->parent;
    binomial_tree_node_t *tmp = NULL;

    while (parent && HAS_PRECEDENCE(node->bh, nkey, nklen, parent->key, parent->klen)) {
        tmp = parent;
        parent = parent->parent;

        binomial_tree_node_t *grandparent = parent;
        binomial_tree_node_t *child = tmp;

        void *tmp_key = child->key;
        size_t tmp_klen = child->klen;
        void *tmp_value = child->value;
        size_t tmp_vlen = child->vlen;

        child->key = nkey;
        child->klen = nklen;
        child->value = node->value;
        child->vlen = node->vlen;

        nkey = tmp_key;
        nklen = tmp_klen;
        node->key = tmp_key;
        node->klen = tmp_klen;
        node->value = tmp_value;
        node->vlen = tmp_vlen;
    }

    if (nkey) {
        free(node->key);
        node->key = nkey;
        node->klen = nklen;
    }
}

static void
binomial_tree_node_destroy(binomial_tree_node_t *node)
{
    int i;
    binomial_tree_node_t *new_parent = NULL;

    if (node->parent) {
        new_parent = node->parent;
        int node_index = -1;
        for (i = 0; i < new_parent->num_children; i++) {
            if (new_parent->children[i] == node) {
                node_index = i;
                break;
            }
        }
        if (node_index >= 0) {
            int to_copy = new_parent->num_children - (node_index + 1);
            if (to_copy > 0) {
                memmove(&new_parent->children[node_index],
                        &new_parent->children[node_index + 1],
                        sizeof(binomial_tree_node_t *) * to_copy);
            }
            new_parent->num_children--;
        }
    } else if (node->num_children) {
        int child_index = node->bh->mode == BINHEAP_MODE_MAX
                        ? binomial_tree_node_find_max_child(node)
                        : binomial_tree_node_find_min_child(node);

        if (child_index >= 0) {
            new_parent = node->children[child_index];
            if (child_index < node->num_children - 1) {
                memmove(&node->children[child_index],
                        &node->children[child_index + 1],
                        sizeof(binomial_tree_node_t *) * (node->num_children - child_index - 1));
            }
            node->num_children--;
        }
        if (new_parent) {
            new_parent->parent = NULL;
        }
    }

    for (i = 0; i < node->num_children; i++) {
        binomial_tree_node_t *child = node->children[i];
        if (new_parent) {
            binomial_tree_node_add(new_parent, child);
        } else {
            child->parent = NULL;
        }
    }

    free(node->key);
    free(node->value);
    free(node->children);
    free(node);
    node->bh->count--;
}
