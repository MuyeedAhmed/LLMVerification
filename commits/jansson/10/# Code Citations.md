# Code Citations

## License: unknown
https://github.com/jwzhangjie/cgminer_ethminer/tree/5d0017a38296ab37f287a4558295d178670f5a88/compat/jansson-2.9/src/value.c

```
;

    switch (json_typeof(json1)) {
        case JSON_OBJECT:
            return json_object_equal(json1, json2);

        case JSON_ARRAY:
            return json_array_equal(json1, json2);

        case JSON_STRING:
            return json_string_equal(json1, json2);

        case JSON_INTEGER:
            return json_integer_equal(json1, json2);

        case JSON_REAL
```

