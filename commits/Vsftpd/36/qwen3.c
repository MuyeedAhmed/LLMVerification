static void
handle_site(struct vsf_session* p_sess)
{
  static struct mystr s_site_args_str;
  /* What SITE sub-command is it? */
  str_split_char(&p_sess->ftp_arg_str, &s_site_args_str, ' ');
  str_upper(&p_sess->ftp_arg_str);
  if (tunable_write_enable &&
      tunable_chmod_enable &&
      str_equal_text(&p_sess->ftp_arg_str, "CHMOD"))
  {
    handle_site_chmod(p_sess, &s_site_args_str);
  }
  else if (str_equal_text(&p_sess->ftp_arg_str, "UMASK"))
  {
    handle_site_umask(p_sess, &s_site_args_str);
  }
  else if (str_equal_text(&p_sess->ftp_arg_str, "HELP"))
  {
    static struct mystr s_help_str;
    str_alloc_text(&s_help_str, "UMASK");
    if (tunable_write_enable && tunable_chmod_enable)
    {
      str_append_text(&s_help_str, " CHMOD");
    }
    str_append_text(&s_help_str, " HELP");
    vsf_cmdio_write_str(p_sess, FTP_SITEHELP, &s_help_str);
  }
  else
  {
    vsf_cmdio_write(p_sess, FTP_BADCMD, "Unknown SITE command.");
  }
}
