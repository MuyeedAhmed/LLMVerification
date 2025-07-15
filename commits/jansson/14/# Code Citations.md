# Code Citations

## License: unknown
https://github.com/nhattieunhatmong/test/tree/ac27ca914ffb6b6715eed522ddf54ea7b0f698f4/compat/jansson/value.c

```
*key, json_t *value)
{
    json_object_t *object;
    object_key_t *k;

    if (!key || !value)
        return -1;

    if (!json_is_object(json) || json == value)
    {
        json_decref(value);
        return -1;
    }

    object
```


## License: unknown
https://github.com/xonglennao/moai-sdk-dev/tree/ac27dd14cdcb40e2f7c31d422b83ec1710112f5c/3rdparty/jansson-2.1/src/value.c

```
{
    json_object_t *object;
    object_key_t *k;

    if (!key || !value)
        return -1;

    if (!json_is_object(json) || json == value)
    {
        json_decref(value);
        return -1;
    }

    object = json_to_object(json);

    /
```

