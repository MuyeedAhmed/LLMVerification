int
vsf_ftpdataio_post_mark_connect(struct vsf_session* p_sess)
{
  int ret = 0;
  if (!p_sess->data_use_ssl)
  {
    return 1;
  }
  if (!p_sess->ssl_slave_active)
  {
    ret = ssl_accept(p_sess, p_sess->data_fd);
  }
  else
  {
    int sock_ret;
    priv_sock_send_cmd(p_sess->ssl_consumer_fd, PRIV_SOCK_DO_SSL_HANDSHAKE);
    priv_sock_send_fd(p_sess->ssl_consumer_fd, p_sess->data_fd);
    sock_ret = priv_sock_get_result(p_sess->ssl_consumer_fd);
    if (sock_ret == PRIV_SOCK_RESULT_OK)
    {
      ret = 1;
    }
  }
  if (ret != 1)
  {
    static struct mystr s_err_msg;
    str_alloc_text(&s_err_msg, "SSL connection failed");
    if (tunable_require_ssl_reuse)
    {
      str_append_text(&s_err_msg, "; session reuse required");
      str_append_text(
          &s_err_msg, ": see require_ssl_reuse option in vsftpd.conf man page");
    }
    vsf_cmdio_write_str(p_sess, FTP_DATATLSBAD, &s_err_msg);

    /* Kill the FTP session on session re-use failure */
    p_sess->goodbye = 1;
    p_sess->terminate = 1;
  }
  return ret;
}