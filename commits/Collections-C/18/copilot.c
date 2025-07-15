/**
 * Removes the element at the specified index in the deque and optionally
 * sets the `out` parameter to the value of the removed element.
 *
 * @param[in] deque the deque from which the element is being removed
 * @param[in] index the index of the element to remove
 * @param[out] out pointer to where the removed element is stored, or NULL
 *                 if it is to be ignored
 *
 * @return CC_OK if the element was successfully removed, or CC_ERR_OUT_OF_RANGE
 * if the index is out of bounds.
 */
enum cc_stat cc_deque_remove_at(CC_Deque *deque, size_t index, void **out)
{
    if (index >= deque->size) {
        // Ensure the index is within bounds
        return CC_ERR_OUT_OF_RANGE;
    }

    // Calculate the actual position in the circular buffer
    size_t p = (deque->head + index) % deque->capacity;

    // Store the removed element in the `out` parameter if provided
    if (out) {
        *out = deque->buffer[p];
    }

    // Shift elements to fill the gap created by the removed element
    if (index < deque->size / 2) {
        // Shift elements from the head to `p` to the right
        for (size_t i = p; i != deque->head; i = (i - 1 + deque->capacity) % deque->capacity) {
            deque->buffer[i] = deque->buffer[(i - 1 + deque->capacity) % deque->capacity];
        }
        deque->head = (deque->head + 1) % deque->capacity;
    } else {
        // Shift elements from `p` to the tail to the left
        for (size_t i = p; i != deque->tail; i = (i + 1) % deque->capacity) {
            deque->buffer[i] = deque->buffer[(i + 1) % deque->capacity];
        }
        deque->tail = (deque->tail - 1 + deque->capacity) % deque->capacity;
    }

    deque->size--; // Decrease the size of the deque
    return CC_OK;
}