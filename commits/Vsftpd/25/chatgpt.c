int str_all_space(const struct mystr* p_str) {
    for (unsigned int i = 0; i < p_str->len; i++) {
        if (!vsf_sysutil_isspace(p_str->p_buf[i])) {
            return 0;
        }
    }
    return 1;
}