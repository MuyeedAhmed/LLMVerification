# Code Citations

## License: unknown
https://github.com/MerlinRdev/K3-merlin.ng/tree/65854733bd36dd4e1c0fbe6c1202e96beaf56389/release/src/router/jansson-2.7/src/pack_unpack.c

```
, va_list *ap)
{
    json_t *object = json_object();
    next_token(s);

    while (token(s) != '}') {
        char *key;
        size_t len;
        int ours;
        json_t *value;

        if (!token(s)) {
```


## License: GPL_3_0
https://github.com/Hincoin/hinminer-gpu/tree/0f22f5dfb27562f4c7c53a4eed10fffc1d2de639/compat/jansson-2.5/src/pack_unpack.c

```
;
        json_t *value;

        if (!token(s)) {
            set_error(s, "<format>", "Unexpected end of format string");
            goto error;
        }

        if (token(s) != 's') {
            set_error(s, "
```

