tagged_value_t *get_tagged_value(linked_list_t *list, char *tag) 
{
    int i;
    tagged_value_t *tval;

    for (i = 0; i < (int)list_count(list); i++) 
    {
        tval = pick_tagged_value(list, i);

        // Check for NULL to prevent segfaults
        if (tval == NULL) {
            continue; // Skip to the next iteration if NULL
        }

        if (strcmp(tval->tag, tag) == 0) {
            return tval;
        }
    }

    return NULL; // Return NULL if no matching tagged value is found
}