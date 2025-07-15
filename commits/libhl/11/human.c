static void
binomial_tree_node_increase_key(binomial_tree_node_t *node, int incr)
{
    void *okey = node->key;
    size_t oklen = node->klen;
    void *nkey = NULL;
    size_t nklen = 0;

    if (incr == 0)
        return;
    else if (incr > 0)
        node->bh->cbs->incr(okey, oklen, &nkey, &nklen, incr);
    else 
        node->bh->cbs->decr(okey, oklen, &nkey, &nklen, -incr);

    binomial_tree_node_t *parent = node->parent;

    int swapped = 0;
    while (parent && HAS_PRECEDENCE(node->bh, nkey, nklen, parent->key, parent->klen))
    {
        binomial_tree_node_t tmp;
        tmp.key = parent->key;
        tmp.klen = parent->klen;
        tmp.value = parent->value;
        tmp.vlen = parent->vlen;

        parent->key = nkey;
        parent->klen = nklen;
        parent->value = node->value;
        parent->vlen = node->vlen;

        node->key = tmp.key;
        node->klen = tmp.klen;
        node->value = tmp.value;
        node->vlen = tmp.vlen;

        binomial_tree_node_t *op = parent;
        parent = parent->parent; 
        node = op;
        swapped++;
    }

    if (!swapped) {
        node->key = nkey;
        node->klen = nklen;
    }

    free(okey);
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
        if (new_parent->num_children && node_index >= 0) {
            int to_copy = new_parent->num_children - (node_index + 1);
            if (to_copy) {
                memcpy(&new_parent->children[node_index],
                       &new_parent->children[node_index+1],
                       sizeof(binomial_tree_node_t *) * to_copy);
            } else {
                // TODO - Error messages
                // (something is badly corrupted if we are here)
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
                memcpy(&node->children[child_index],
                       &node->children[child_index + 1],
                       sizeof(binomial_tree_node_t *) * (node->num_children - child_index + 1));
                       
            }
            node->num_children--;
        }
        new_parent->parent = NULL;
    }

    for (i = 0; i < node->num_children; i++) {
        if (new_parent)
            binomial_tree_node_add(new_parent, node->children[i]);
        else
            node->children[i]->parent = NULL;
    }

    free(node->key);
    node->bh->count--;
    free(node);
}