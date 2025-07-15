static void get_unique_filename(struct mystr* p_outstr, const struct mystr* p_base_str)
{
    static struct vsf_sysutil_statbuf* s_p_statbuf;
    unsigned int suffix = 1;
    struct mystr base_with_prefix;

    if (str_isempty(p_base_str)) {
        // STOU with no filename, use "STOU.<suffix>"
        str_alloc_text(&base_with_prefix, "STOU");
    } else {
        // Copy the original base string
        str_copy(&base_with_prefix, p_base_str);
    }

    str_append_char(&base_with_prefix, '.');
    
    int retval = str_stat(&base_with_prefix, &s_p_statbuf);
    if (vsf_sysutil_retval_is_error(retval))
    {
        str_copy(p_outstr, &base_with_prefix);
        return;
    }

    while (1)
    {
        str_copy(p_outstr, &base_with_prefix);
        str_append_ulong(p_outstr, suffix);
        retval = str_stat(p_outstr, &s_p_statbuf);
        if (vsf_sysutil_retval_is_error(retval))
        {
            return;
        }
        ++suffix;
    }
}