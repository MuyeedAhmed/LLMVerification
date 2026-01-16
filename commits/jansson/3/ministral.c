#include <stdlib.h>
#include <string.h>

#include "jansson_private.h"
#include "hashtable.h"

void json_object_clear(json_t *json)
{
    json_object_t *object;

    if (!json_is_object(json))
        return -1;

    object = json_to_object(json);

    while (hashtable_size(&object->hashtable) > 0) {
        void *iter = hashtable_iter(&object->hashtable);
        if (iter) {
            const char *key = hashtable_iter_key(iter);
            json_t *value = (json_t *)hashtable_iter_value(iter);
            hashtable_del(&object->hashtable, key);
            json_decref(value);
        }
    }
}
