static void get_unique_filename(struct mystr* p_outstr, const struct mystr* p_base_str)
{
  static struct vsf_sysutil_statbuf* s_p_statbuf;
  unsigned int suffix = 1;
  static struct mystr s_prefix_str;
  static struct mystr s_filename_str;
  const char* p_base_buf = str_getbuf(p_base_str);
  int is_stou = 0;
  int len = str_getlen(p_base_str);
  
  if (len >= 4 && !strncmp(p_base_buf, "STOU", 4))
  {
    is_stou = 1;
    str_alloc_text(&s_prefix_str, "STOU");
  }
  else
  {
    str_copy(&s_prefix_str, p_base_str);
  }
  
  if (is_stou)
  {
    str_copy(&s_filename_str, &s_prefix_str);
    str_append_char(&s_filename_str, '.');
    str_append_ulong(&s_filename_str, suffix);
    str_copy(p_outstr, &s_filename_str);
    return;
  }
  
  /* Use silly wu-ftpd algorithm for compatibility. It has races of course, if
   * two sessions are using the same file prefix at the same time.
   */
  int retval = str_stat(p_base_str, &s_p_statbuf);
  if (vsf_sysutil_retval_is_error(retval))
  {
    str_copy(p_outstr, p_base_str);
    return;
  }
  while (1)
  {
    str_copy(p_outstr, p_base_str);
    str_append_char(p_outstr, '.');
    str_append_ulong(p_outstr, suffix);
    retval = str_stat(p_outstr, &s_p_statbuf);
    if (vsf_sysutil_retval_is_error(retval))
    {
      return;
    }
    ++suffix;
  }
}
