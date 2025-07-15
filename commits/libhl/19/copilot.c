int
fbuf_remove(fbuf_t *fbuf, unsigned int len)
{
    if (len >= fbuf->used) {
        fbuf->used = 0;
        fbuf->skip = 0;
        fbuf->data[0] = '\0'; // terminate the buffer string
    } else if (len) {
        fbuf->skip += len;
        fbuf->used -= len;
        if (fbuf->skip > fbuf->len / 2) {
            memmove(fbuf->data, fbuf->data + fbuf->skip, fbuf->used + 1);
            fbuf->skip = 0; // Reset the skip pointer after shifting data
        }
    }
    return fbuf->used;
}