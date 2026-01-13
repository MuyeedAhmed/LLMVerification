No explanation needed.

```c
static int read_len_table(uint8_t *dst, GetByteContext *gb, int n)
{
    int i, val, repeat;

    for (i = 0; i < n;) {
        if (bytestream2_get_bytes_left(gb) <= 0)
            goto error;
        repeat = bytestream2_peek_byteu(gb) >> 5;
        val    = bytestream2_get_byteu(gb) & 0x1F;
        if (repeat == 0) {
            if (bytestream2_get_bytes_left(gb) <= 0)
                goto error;
            repeat = bytestream2_get_byteu(gb);
        }
        av_assert0(i + repeat <= n);
        while (repeat--)
            dst[i++] = val;
    }
    return 0;

error:
    av_log(NULL, AV_LOG_ERROR, "Error reading huffman table\n");
    return AVERROR_INVALIDDATA;
}
```
