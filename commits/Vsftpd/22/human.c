static void
get_unique_filename(struct mystr* p_outstr, const struct mystr* p_base_str)
{
  /* Use silly wu-ftpd algorithm for compatibility. It has races of course, if
   * two sessions are using the same file prefix at the same time.
   */
  static struct vsf_sysutil_statbuf* s_p_statbuf;
  static struct mystr s_stou_str;
  unsigned int suffix = 1;
  const struct mystr* p_real_base_str = p_base_str;
  int retval;
  if (str_isempty(p_real_base_str))
  {
    str_alloc_text(&s_stou_str, "STOU");
    p_real_base_str = &s_stou_str;
  }
  else
  {
    /* Do not add any suffix at all if the name is not taken. */
    retval = str_stat(p_real_base_str, &s_p_statbuf);
    if (vsf_sysutil_retval_is_error(retval))
    {
       str_copy(p_outstr, p_real_base_str);
       return;
    }
  }
  while (1)
  {
    str_copy(p_outstr, p_real_base_str);
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