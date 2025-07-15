/**
 * Adds a new element to the array at a specified position by shifting all
 * subsequent elements by one. The specified index must be within the bounds
 * of the array, otherwise this operation will fail and false will be
 * returned to indicate failure. This function may also fail if the memory
 * allocation for the new element was unsuccessful.
 *
 * @param[in] ar the Array to which the element is being added
 * @param[in] element the element that is being added
 * @param[in] index the position in the array at which the element is being
 *            added
 *
 * @return true if the operation was successful
 */
bool array_add_at(Array *ar, void *element, size_t index)
{
    if (index > ar->size) // Ensure index is within bounds
        return false;

    if (ar->size == ar->capacity && !expand_capacity(ar)) // Expand capacity if needed
        return false;

    // Check for potential overflow in the shift calculation
    if (ar->size > SIZE_MAX / sizeof(void*)) {
        return false; // Prevent overflow
    }

    size_t shift = (ar->size - index) * sizeof(void*);

    // Perform the shift using memmove
    memmove(&(ar->buffer[index + 1]),
            &(ar->buffer[index]),
            shift);

    ar->buffer[index] = element;
    ar->size++;

    return true;
}