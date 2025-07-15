/* linked list management library - by xant 
 */
 
 #include <stdio.h>
 #ifdef THREAD_SAFE
 #include <pthread.h>
 #endif
 #include "linklist.h"
 #include <string.h>
 #include <errno.h>
 
 typedef struct __list_entry {
     struct __linked_list *list;
     struct __list_entry *prev;
     struct __list_entry *next;
     void *value;
     int tagged;
 } list_entry_t;
 
 struct __linked_list {
     list_entry_t *head;
     list_entry_t *tail;
     list_entry_t *cur;
     uint32_t  pos;
     uint32_t length;
 #ifdef THREAD_SAFE
     pthread_mutex_t lock;
 #endif
     int free;
     free_value_callback_t free_value_cb;
 };
 
 #ifndef PTHREAD_MUTEX_RECURSIVE
 #ifdef PTHREAD_MUTEX_RECURSIVE_NP
 #define PTHREAD_MUTEX_RECURSIVE PTHREAD_MUTEX_RECURSIVE_NP
 #endif
 #endif
 
 /********************************************************************
  * Entry-based API   
  * - Internal use only
  ********************************************************************/
 
 /* Entry creation and destruction routines */
 static inline list_entry_t *create_entry();
 static inline void destroy_entry(list_entry_t *entry);
 
 /* List and list_entry_t manipulation routines */
 static inline list_entry_t *pop_entry(linked_list_t *list);
 static inline int push_entry(linked_list_t *list, list_entry_t *entry);
 static inline int unshift_entry(linked_list_t *list, list_entry_t *entry);
 static inline list_entry_t *shift_entry(linked_list_t *list);
 static inline int insert_entry(linked_list_t *list, list_entry_t *entry, uint32_t pos);
 static inline list_entry_t *pick_entry(linked_list_t *list, uint32_t pos);
 static inline list_entry_t *fetch_entry(linked_list_t *list, uint32_t pos);
 //list_entry_t *SelectEntry(linked_list_t *list, uint32_t pos);
 static inline list_entry_t *remove_entry(linked_list_t *list, uint32_t pos);
 static inline long get_entry_position(list_entry_t *entry);
 static inline int move_entry(linked_list_t *list, uint32_t srcPos, uint32_t dstPos);
 static inline list_entry_t *subst_entry(linked_list_t *list, uint32_t pos, list_entry_t *entry);
 static inline int swap_entries(linked_list_t *list, uint32_t pos1, uint32_t pos2);
 
 #ifdef THREAD_SAFE
 #define MUTEX_LOCK(__mutex) pthread_mutex_lock(__mutex) 
 #define MUTEX_UNLOCK(__mutex) pthread_mutex_unlock(__mutex) 
 #else
 #define MUTEX_LOCK(__mutex)
 #define MUTEX_UNLOCK(__mutex)
 #endif
 
 /* 
  * Create a new linked_list_t. Allocates resources and returns 
  * a linked_list_t opaque structure for later use 
  */
 linked_list_t *create_list() 
 {
     linked_list_t *list = (linked_list_t *)malloc(sizeof(linked_list_t));
     if(list) {
         init_list(list);
         list->free = 1;
     } else {
         //fprintf(stderr, "Can't create new linklist: %s", strerror(errno));
         return NULL;
     }
     return list;
 }
 
 /*
  * Initialize a preallocated linked_list_t pointed by list 
  * useful when using static list handlers
  */ 
 void init_list(linked_list_t *list) 
 {
     memset(list,  0, sizeof(linked_list_t));
 #ifdef THREAD_SAFE
     pthread_mutexattr_t attr;
     pthread_mutexattr_init(&attr);
     pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
     pthread_mutex_init(&list->lock, &attr);
     pthread_mutexattr_destroy(&attr);
 #endif
 
 }
 
 /*
  * Destroy a linked_list_t. Free resources allocated for list
  */
 void destroy_list(linked_list_t *list) 
 {
     if(list) 
     {
         clear_list(list);
 #ifdef THREAD_SAFE
         pthread_mutex_destroy(&list->lock);
 #endif
         if(list->free) free(list);
     }
 }
 
 /*
  * Clear a linked_list_t. Removes all entries in list
  * Dangerous if used with value-based api ... 
  * if values are associated to entries, resources for those will not be freed.
  * clear_list() can be used safely with entry-based and tagged-based api,
  * otherwise you must really know what you are doing
  */
 void clear_list(linked_list_t *list) 
 {
     list_entry_t *e;
     /* Destroy all entries still in list */
     while((e = shift_entry(list)) != NULL)
     {
         /* if there is a tagged_value_t associated to the entry, 
         * let's free memory also for it */
         if(e->tagged && e->value)
             destroy_tagged_value((tagged_value_t *)e->value);
         else if (list->free_value_cb)
             list->free_value_cb(e->value);
         
         destroy_entry(e);
     }
 }
 
 /* Returns actual lenght of linked_list_t pointed by l */
 uint32_t list_count(linked_list_t *l) 
 {
     uint32_t len;
     MUTEX_LOCK(&l->lock);
     len = l->length;
     MUTEX_UNLOCK(&l->lock);
     return len;
 }
 
 void set_free_value_callback(linked_list_t *list, free_value_callback_t free_value_cb) {
     MUTEX_LOCK(&list->lock);
     list->free_value_cb = free_value_cb;
     MUTEX_UNLOCK(&list->lock);
 }
 
 void list_lock(linked_list_t *list) {
     MUTEX_LOCK(&list->lock);
 }
 
 void list_unlock(linked_list_t *list) {
     MUTEX_UNLOCK(&list->lock);
 }
 
 /* 
  * Create a new list_entry_t structure. Allocates resources and returns  
  * a pointer to the just created list_entry_t opaque structure
  */
 static inline list_entry_t *create_entry() 
 {
     list_entry_t *new_entry = (list_entry_t *)calloc(1, sizeof(list_entry_t));
     /*
     if (!new_entry) {
         fprintf(stderr, "Can't create new entry: %s", strerror(errno));
     }
     */
     return new_entry;
 }
 
 /* 
  * Free resources allocated for a list_entry_t structure. 
  * If the entry is linked in a list this routine will also unlink correctly
  * the entry from the list.
  */
 static inline void destroy_entry(list_entry_t *entry) 
 {
     long pos;
     if(entry) 
     {
         if(entry->list) 
         {
             /* entry is linked in a list...let's remove that reference */
             pos = get_entry_position(entry);
             if(pos >= 0)
                 remove_entry(entry->list, pos);
         }
         free(entry);
     }
 }
 
 /*
  * Pops a list_entry_t from the end of the list (or bottom of the stack
  * if you are using the list as a stack)
  */
 static inline list_entry_t *pop_entry(linked_list_t *list) 
 {
     list_entry_t *entry;
     MUTEX_LOCK(&list->lock);
 
     entry = list->tail;
     if(entry) 
     {
         list->tail = entry->prev;
         if(list->tail)
             list->tail->next = NULL;
         list->length--;
 
         entry->list = NULL;
         entry->prev = NULL;
         entry->next = NULL;
 
         if (list->cur == entry)
             list->cur = NULL;
     }
     if(list->length == 0)
         list->head = list->tail = NULL;
 
     MUTEX_UNLOCK(&list->lock);
     return entry;
 }
 
 /*
  * Pushs a list_entry_t at the end of a list
  */
 static inline int push_entry(linked_list_t *list, list_entry_t *entry) 
 {
     list_entry_t *p;
     if(!entry)
         return -1;
     MUTEX_LOCK(&list->lock);
     if(list->length == 0) 
     {
         list->head = list->tail = entry;
     }
     else 
     {
         p = list->tail;
         p->next = entry;
         entry->prev = p;
         entry->next = NULL;
         list->tail = entry;
     }
     list->length++;
     entry->list = list;
     MUTEX_UNLOCK(&list->lock);
     return 0;
 }
  
 /*
  * Retreive a list_entry_t from the beginning of a list (or top of the stack
  * if you are using the list as a stack) 
  */
 static inline list_entry_t *shift_entry(linked_list_t *list) 
 {
     list_entry_t *entry;
     MUTEX_LOCK(&list->lock);
     entry = list->head;
     if(entry) 
     {
         list->head = entry->next;
         if(list->head) 
             list->head->prev = NULL;
         list->length--;
 
         entry->list = NULL;
         entry->prev = NULL;
         entry->next = NULL;
 
         if (list->cur == entry)
             list->cur = NULL;
         else if (list->pos)
             list->pos--;
     }
     if(list->length == 0)
         list->head = list->tail = NULL;
     MUTEX_UNLOCK(&list->lock);
     return entry;
 }
 
 
 /* 
  * Insert a list_entry_t at the beginning of a list (or at the top if the stack)
  */
 static inline int unshift_entry(linked_list_t *list, list_entry_t *entry) 
 {
     list_entry_t *p;
     if(!entry)
         return -1;
     MUTEX_LOCK(&list->lock);
     if(list->length == 0) 
     {
         list->head = list->tail = entry;
     } 
     else 
     {
         p = list->head;
         p->prev = entry;
         entry->prev = NULL;
         entry->next = p;
         list->head = entry;
     }
     list->length++;
     entry->list = list;
     if (list->cur)
         list->pos++;
     MUTEX_UNLOCK(&list->lock);
     return 0;
 }
 
 /*
  * Instert an entry at a specified position in a linked_list_t
  */
 static inline int insert_entry(linked_list_t *list, list_entry_t *entry, uint32_t pos) 
 {
     list_entry_t *prev, *next;
     int ret = -1;
     MUTEX_LOCK(&list->lock);
     if(pos == 0) {
         ret = unshift_entry(list, entry);
     } else if(pos == list->length) {
         ret = push_entry(list, entry);
     } else if (pos > list->length) {
         unsigned int i;
         for (i = list->length; i < pos; i++) {
             list_entry_t *emptyEntry = create_entry();
             push_entry(list, emptyEntry);
         }
         ret = push_entry(list, entry);
     }
 
     if (ret == 0) {
         MUTEX_UNLOCK(&list->lock);
         return ret;
     }
 
     prev = pick_entry(list, pos-1);
     if(prev) 
     {
         next = prev->next;
         prev->next = entry;
         entry->prev = prev;
         entry->next = next;
         if (next)
             next->prev = entry;
         list->length++;
         ret = 0;
     }
     MUTEX_UNLOCK(&list->lock);
     return ret;
 }
 
 /* 
  * Retreive the list_entry_t at pos in a linked_list_t without removing it from the list
  */
 static inline list_entry_t *pick_entry(linked_list_t *list, uint32_t pos) 
 {
     unsigned int i;
     list_entry_t *entry;
     if(list->length <= pos)
         return NULL;
 
     uint32_t half_length = list->length >> 1;
     MUTEX_LOCK(&list->lock);
     if (list->cur && (uint32_t)abs(list->pos - pos) < half_length) {
         entry = list->cur;
         if (list->pos != pos) {
             if (list->pos < pos) {
                 for(i=list->pos; i < pos; i++)  {
                     entry = entry->next;
                 }
             } else if (list->pos > pos) {
                 for(i=list->pos; i > pos; i--)  {
                     entry = entry->prev;
                 }
             }
         }
     } else {
         if (pos > half_length) 
         {
             entry = list->tail;
             for(i=list->length - 1;i>pos;i--)  {
                 entry = entry->prev;
             }
         }
         else 
         {
             entry = list->head;
             for(i=0;i<pos;i++) 
                 entry = entry->next;
         }
     }
     if (entry) {
         list->pos = pos;
         list->cur = entry;
     }
     MUTEX_UNLOCK(&list->lock);
     return entry;
 }
 
 /* Retreive the list_entry_t at pos in a linked_list_t removing it from the list 
  * XXX - no locking here because this routine is just an accessor to other routines
  * XXX - POSSIBLE RACE CONDITION BETWEEN pick_entry and remove_entry
  * Caller MUST destroy returned entry trough destroy_entry() call
  */
 static inline list_entry_t *fetch_entry(linked_list_t *list, uint32_t pos) 
 {
     list_entry_t *entry = NULL;
     if(pos == 0 )
         return shift_entry(list);
     else if(pos == list->length - 1)
         return pop_entry(list);
 
     entry = remove_entry(list, pos); 
     return entry;
 }
 
 /*
 list_entry_t *SelectEntry(linked_list_t *list, uint32_t pos) 
 {
 }
 */
 
 static inline int move_entry(linked_list_t *list, uint32_t srcPos, uint32_t dstPos) 
 {
     list_entry_t *e;
     
     e = fetch_entry(list, srcPos);
     if(e)
     {
         if(insert_entry(list, e, dstPos) == 0)
             return 0;
         else 
         {
             if(insert_entry(list, e, srcPos) != 0)
             {
                 //fprintf(stderr, "Can't restore entry at index %lu while moving to %lu\n", srcPos, dstPos);
             }
         }
     }
     /* TODO - Unimplemented */
     return -1;
 }
 
 /* XXX - still dangerous ... */
 static inline int swap_entries(linked_list_t *list, uint32_t pos1, uint32_t pos2) 
 {
     list_entry_t *e1;
     list_entry_t *e2;
     if(pos2 > pos1)
     {
         e2 = fetch_entry(list, pos2);
         insert_entry(list, e2, pos1);
         e1 = fetch_entry(list,  pos1+1);
         insert_entry(list, e1, pos2);
     }
     else if(pos1 > pos2)
     {
         e1 = fetch_entry(list, pos1);
         insert_entry(list, e1, pos2);
         e2 = fetch_entry(list, pos2+1);
         insert_entry(list, e2, pos1);
     }
     else
         return -1;
     
     /* TODO - Unimplemented */
     return 0;
 }
 
 /* return old entry at pos */
 static inline list_entry_t *subst_entry(linked_list_t *list, uint32_t pos, list_entry_t *entry)
 {
     list_entry_t *old;
 
     MUTEX_LOCK(&list->lock);
 
     old = fetch_entry(list,  pos);
     if(!old)
         return NULL;
     insert_entry(list, entry, pos);
 
     MUTEX_UNLOCK(&list->lock);
     /* XXX - NO CHECK ON INSERTION */
     return old;
 }
 
 static inline list_entry_t *remove_entry(linked_list_t *list, uint32_t pos) 
 {
     list_entry_t *next, *prev;
     list_entry_t *entry = pick_entry(list, pos);
     MUTEX_LOCK(&list->lock);
     if(entry) 
     {
         prev = entry->prev;
         next = entry->next;
         if (pos == 0)
             list->head = next;
         else if (pos == list->length - 1)
             list->tail = prev;
 
         if(prev) 
             prev->next = next;
         if(next)
             next->prev = prev;
 
         list->length--;
         entry->list = NULL;
         entry->prev = NULL;
         entry->next = NULL;
 
         if (list->cur == entry) {
             list->cur = NULL;
             list->pos = 0;
         } else if (list->pos > pos) {
             list->pos--;
         }
         MUTEX_UNLOCK(&list->lock);
         return entry;
     }
     MUTEX_UNLOCK(&list->lock);
     return NULL;
 }
 
 /* return position of entry if linked in a list.
  * Scans entire list so it can be slow for very long lists */
 long get_entry_position(list_entry_t *entry) 
 {
     int i = 0;
     linked_list_t *list;
     list_entry_t *p;
     list = entry->list;
     if(list) 
     {
         p  = list->head;
         while(p) 
         {
             if(p == entry) return i;
             p = p->next;
             i++;
         }
     }
     return -1;
 }
 
 void *pop_value(linked_list_t *list) 
 {
     void *val = NULL;
     list_entry_t *entry = pop_entry(list);
     if(entry) 
     {
         val = entry->value;
         destroy_entry(entry);
     }
     return val;
 }
 
 int push_value(linked_list_t *list, void *val) 
 {
     int res;
     list_entry_t *new_entry = create_entry();
     if(!new_entry)
         return -1;
     new_entry->value = val;
     res = push_entry(list, new_entry);
     if(res != 0)
         destroy_entry(new_entry);
     return res;
 }
 
 int unshift_value(linked_list_t *list, void *val) 
 {
     int res;
     list_entry_t *new_entry = create_entry();
     if(!new_entry)
         return -1;
     new_entry->value = val;
     res = unshift_entry(list, new_entry);
     if(res != 0)
         destroy_entry(new_entry);
     return res;
 }
 
 void *shift_value(linked_list_t *list) 
 {
     void *val = NULL;
     list_entry_t *entry = shift_entry(list);
     if(entry) 
     {
         val = entry->value;
         destroy_entry(entry);
     }
     return val;
 }
 
 int insert_value(linked_list_t *list, void *val, uint32_t pos) 
 {
     int res;
     list_entry_t *new_entry = create_entry();
     if(!new_entry)
         return -1;
     new_entry->value = val;
     res=insert_entry(list, new_entry, pos);
     if(res != 0)
         destroy_entry(new_entry);
     return res;
 }
 
 void *pick_value(linked_list_t *list, uint32_t pos) 
 {
     list_entry_t *entry = pick_entry(list, pos);
     if(entry)
         return entry->value;
     return NULL;
 }
 
 void *fetch_value(linked_list_t *list, uint32_t pos) 
 {
     void *val = NULL;
     list_entry_t *entry = fetch_entry(list, pos);
     if(entry) 
     {
         val = entry->value;
         destroy_entry(entry);
     }
     return val;
 }
 
 /* just an accessor to move_entry */
 int move_value(linked_list_t *list, uint32_t srcPos, uint32_t dstPos)
 {
     return move_entry(list, srcPos, dstPos);
 }
 
 void *set_value(linked_list_t *list, uint32_t pos, void *newval)
 {
     void *old_value = NULL;
     MUTEX_LOCK(&list->lock);
     list_entry_t *entry = pick_entry(list, pos);
     if (entry) {
         old_value = entry->value;
         entry->value = newval;
     } else {
         insert_value(list, newval, pos);
     }
     MUTEX_UNLOCK(&list->lock);
     return old_value;
 }
 
 /* return old value at pos */
 void *subst_value(linked_list_t *list, uint32_t pos, void *newval)
 {
     void *old_value = NULL;
     MUTEX_LOCK(&list->lock);
     list_entry_t *entry = pick_entry(list, pos);
     if (entry) {
         old_value = entry->value;
         entry->value = newval;
     }
     MUTEX_UNLOCK(&list->lock);
     return old_value;
 }
 
 int swap_values(linked_list_t *list,  uint32_t pos1, uint32_t pos2)
 {
     return swap_entries(list, pos1, pos2);
 }
 
 void foreach_list_value(linked_list_t *list, int (*item_handler)(void *item, uint32_t idx, void *user), void *user)
 {
     MUTEX_LOCK(&list->lock);
 #if 0
     for(i=0;i<list->length;i++) {
         if (item_handler(pick_value(list, i), i, user) == 0)
             break;
     }
 #endif
     uint32_t idx = 0;
     list_entry_t *e = list->head;
     while(e) {
         int rc = item_handler(e->value, idx++, user);
         if (rc == 0) {
             break;
         } else if (rc == -1) {
             list_entry_t *d = e;
             if (list->head == list->tail && list->tail == e) {
                 list->head = list->tail = NULL;
             } else if (e == list->head) {
                 list->head = e->next;
                 list->head->prev = NULL;
             } else if (e == list->tail) {
                 list->tail = e->prev;
                 list->tail->next = NULL;
             } else {
                 e = e->next;
                 e->prev = d->prev;
                 e->prev->next = e;
             }
             d->list = NULL;
             list->length--;
             // the callback got the value and will take care of releasing it
             destroy_entry(d);
             break;
         } else {
             e = e->next;
         }
     }
     MUTEX_UNLOCK(&list->lock);
 }
 
 tagged_value_t *create_tagged_value_nocopy(char *tag, void *val) 
 {
     tagged_value_t *newval = (tagged_value_t *)calloc(1, sizeof(tagged_value_t));
     if(!newval) {
         //fprintf(stderr, "Can't create new tagged value: %s", strerror(errno));
         return NULL;
     }
 
     if(tag)
         newval->tag = strdup(tag);
     if (val)
         newval->value = val;
 
     return newval;
 }
 
 /* 
  * Allocates resources for a new tagged_value_t initializing both tag and value
  * to what received as argument.
  * if vlen is 0 or negative, then val is assumed to be a string and 
  * strdup is used to copy it.
  * Return a pointer to the new allocated tagged_value_t.
  */
 tagged_value_t *create_tagged_value(char *tag, void *val, uint32_t vlen) 
 {
     tagged_value_t *newval = (tagged_value_t *)calloc(1, sizeof(tagged_value_t));
     if(!newval) {
         //fprintf(stderr, "Can't create new tagged value: %s", strerror(errno));
         return NULL;
     }
 
     if(tag)
         newval->tag = strdup(tag);
     if(val) 
     {
         if(vlen) 
         {
             newval->value = malloc(vlen+1);
             if(newval->value) 
             {
                 memcpy(newval->value, val, vlen);
                 memset((char *)newval->value+vlen, 0, 1);
                 newval->vlen = vlen;
             } else {
                 //fprintf(stderr, "Can't copy value: %s", strerror(errno));
                 free(newval->tag);
                 free(newval);
                 return NULL;
             }
             newval->type = TV_TYPE_BINARY;
         } 
         else 
         {
             newval->value = (void *)strdup((char *)val);
             newval->vlen = (uint32_t)strlen((char *)val);
             newval->type = TV_TYPE_STRING;
         }
     }
     return newval;
 }
 
 /* 
  * Allocates resources for a new tagged_value_t 
  * containing a linked_list_t instead of a simple buffer.
  * This let us define folded linked_list_t and therefore represent
  * trees (or a sort of folded hashrefs)
  */
 tagged_value_t *create_tagged_sublist(char *tag, linked_list_t *sublist)  
 {
     tagged_value_t *newval = (tagged_value_t *)calloc(1, sizeof(tagged_value_t));
     if(!newval) {
         //fprintf(stderr, "Can't create new tagged value: %s", strerror(errno));
         return NULL;
     }
 
     if(tag)
         newval->tag = strdup(tag);
     newval->type = TV_TYPE_LIST;
     newval->value = sublist;
     return newval;
 }
 
 /* Release resources for tagged_value_t pointed by tval */
 void destroy_tagged_value(tagged_value_t *tval) 
 {
     if(tval) 
     {
         if(tval->tag)
             free(tval->tag);
         if(tval->value) {
             if(tval->type == TV_TYPE_LIST) 
                 destroy_list((linked_list_t *)tval->value);
             else if (tval->vlen)
                 free(tval->value);
         }
         free(tval);
     }
 }
 
 /* Pops a tagged_value_t from the list pointed by list */
 tagged_value_t *pop_tagged_value(linked_list_t *list) 
 {
     return (tagged_value_t *)pop_value(list);
 }
 
 /* 
  * Pushes a new tagged_value_t into list. user must give a valid tagged_value_t pointer 
  * created trough a call to create_tagged_value() routine 
  */
 int push_tagged_value(linked_list_t *list, tagged_value_t *tval) 
 {
     list_entry_t *new_entry;
     int res = 0;
     if(tval) 
     {
         new_entry = create_entry();
         if(new_entry) 
         {
             new_entry->tagged = 1;
             new_entry->value = tval;
             res = push_entry(list, new_entry);
             if(res != 0)
                 destroy_entry(new_entry);
         }
     }
     return res;
 }
 
 int unshift_tagged_value(linked_list_t *list, tagged_value_t *tval) 
 {
     int res = 0;
     list_entry_t *new_entry;
     if(tval) 
     {
         new_entry = create_entry();
         if(new_entry)
          {
             new_entry->tagged = 1;
             new_entry->value = tval;
             res = unshift_entry(list, new_entry);
             if(res != 0)
                 destroy_entry(new_entry);
         }
     }
     return res;
 }
  
 tagged_value_t *shift_tagged_value(linked_list_t *list) 
 {
     return (tagged_value_t *)shift_value(list);
 }
 
 int insert_tagged_value(linked_list_t *list, tagged_value_t *tval, uint32_t pos) 
 {
     int res = 0;
     list_entry_t *new_entry;
     if(tval) 
     {
         new_entry = create_entry();
         if(new_entry) 
         {
             new_entry->tagged = 1;
             new_entry->value = tval;
             res = insert_entry(list, new_entry, pos);
             if(res != 0)
                 destroy_entry(new_entry);
         }
     }
     return res;
 }
 
 tagged_value_t *pick_tagged_value(linked_list_t *list, uint32_t pos) 
 {
     return (tagged_value_t *)pick_value(list, pos);
 }
 
 tagged_value_t *fetch_tagged_value(linked_list_t *list, uint32_t pos) 
 {
     return (tagged_value_t *)fetch_value(list, pos);
 }
 
 /* 
  * ... without removing it from the list
  */
 tagged_value_t *get_tagged_value(linked_list_t *list, char *tag) 
 {
     int i;
     tagged_value_t *tval;
     for(i = 0;i < (int)list_count(list); i++)
     {
         tval = pick_tagged_value(list, i);
         if (!tval) {
             continue;
         }
         if(strcmp(tval->tag, tag) == 0)
             return tval;
     }
     return NULL;
 }
 
 /* 
  * ... without removing it from the list
  * USER MUST NOT FREE MEMORY FOR RETURNED VALUES
  * User MUST create a new list, pass it as 'values'
  * and destroy it when no more needed .... entries 
  * returned inside the 'values' list MUST not be freed, 
  * because they reference directly the real entries inside 'list'.
  */
 uint32_t get_tagged_values(linked_list_t *list, char *tag, linked_list_t *values) 
 {
     int i;
     int ret;
     tagged_value_t *tval;
     ret = 0;
     for(i = 0;i < (int)list_count(list); i++)
     {
         tval = pick_tagged_value(list, i);
         if (!tval) {
             continue;
         }
         if(strcmp(tval->tag, tag) == 0)
         {
             push_value(values, tval->value);
             ret++;
         }
     }
     return ret;
 }
 
 