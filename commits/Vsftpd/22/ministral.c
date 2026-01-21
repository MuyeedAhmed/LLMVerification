static void
get_unique_filename(struct mystr* p_outstr, const struct mystr* p_base_str)
{
    static struct vsf_sysutil_statbuf* s_p_statbuf;
    unsigned int suffix = 1;

    str_copy(p_outstr, p_base_str);
    str_append_text(p_outstr, ".STOU");

    while (1) {
        int retval = str_stat(p_outstr, &s_p_statbuf);
        if (vsf_sysutil_retval_is_error(retval)) {
            return;
        }
        ++suffix;
        str_copy(p_outstr, p_base_str);
        str_append_text(p_outstr, ".STOU");
        str_append_ulong(p_outstr, suffix);
    }
}
