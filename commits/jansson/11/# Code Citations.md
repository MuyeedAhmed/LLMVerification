# Code Citations

## License: unknown
https://github.com/jwzhangjie/cgminer_ethminer/tree/5d0017a38296ab37f287a4558295d178670f5a88/compat/jansson-2.9/src/value.c

```
json_copy(json_t *json)
{
    if (!json)
        return NULL;

    switch (json_typeof(json)) {
        case JSON_OBJECT:
            return json_object_copy(json);

        case JSON_ARRAY:
            return json_array_copy(json);

        case JSON_STRING:
            return json_string_copy(json);

        case JSON_INTEGER:
            return json_integer_copy
```

