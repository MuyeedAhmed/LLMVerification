int
fbuf_copy(fbuf_t *fbufsrc, fbuf_t *fbufdst)
{
    if (!fbuf_extend(fbufdst, fbufsrc->used))
        return -1;
    fbuf_clear(fbufdst);
    memcpy(fbufdst->data + fbufdst->skip, fbufsrc->data + fbufsrc->skip, fbufsrc->used);
    fbufdst->used = fbufsrc->used;
    fbufdst->data[fbufdst->skip+fbufdst->used] = 0;

    return fbufdst->used;
}