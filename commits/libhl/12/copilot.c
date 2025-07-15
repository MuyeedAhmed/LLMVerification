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
            if (to_copy > 0) {
                memcpy(&new_parent->children[node_index],
                       &new_parent->children[node_index + 1],
                       sizeof(binomial_tree_node_t *) * to_copy);
            }
            new_parent->num_children--; // Correctly decrement the number of children
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
                       sizeof(binomial_tree_node_t *) * (node->num_children - child_index - 1)); // Fixed off-by-one error
            }
            node->num_children--; // Correctly decrement the number of children
        }
        new_parent->parent = NULL;
    }

    for (i = 0; i < node->num_children; i++) {
        if (new_parent)
            binomial_tree_node_add(new_parent, node->children[i]);
        else
            node->children[i]->parent = NULL;
    }

    free(node->children); // Ensure children array is freed
    free(node->key);
    node->bh->count--;
    free(node);
}