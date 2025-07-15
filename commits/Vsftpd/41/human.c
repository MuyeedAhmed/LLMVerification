static void
control_getline(struct mystr* p_str, struct vsf_session* p_sess)
{
  int ret;
  if (p_sess->p_control_line_buf == 0)
  {
    vsf_secbuf_alloc(&p_sess->p_control_line_buf, VSFTP_MAX_COMMAND_LINE);
  }
  ret = ftp_getline(p_sess, p_str, p_sess->p_control_line_buf);
  if (ret < 0)
  {
    vsf_cmdio_write_exit(p_sess, FTP_BADCMD, "Input line too long.");
  }
  /* As mandated by the FTP specifications.. */
  str_replace_char(p_str, '\0', '\n');
  /* If the last character is a \r, strip it */
  {
    unsigned int len = str_getlen(p_str);
    while (len > 0 && str_get_char_at(p_str, len - 1) == '\r')
    {
      str_trunc(p_str, len - 1);
      --len;
    }
  }
}