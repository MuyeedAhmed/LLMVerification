bool array_add_at(Array *ar, void *element, size_t index)
 {
     if ((ar->size == 0 && index != 0) || index > (ar->size - 1))
         return false;
 
     if (ar->size == ar->capacity && !expand_capacity(ar))
         return false;
 
     size_t shift = (ar->size - index) * sizeof(void*);
 
     memmove(&(ar->buffer[index + 1]),
             &(ar->buffer[index]),
             shift);
 
     ar->buffer[index] = element;
     ar->size++;
 
     return true;
 }