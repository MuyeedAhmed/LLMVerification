unsigned short
vsf_privop_pasv_listen(struct vsf_session* p_sess)
{
  static struct vsf_sysutil_sockaddr* s_p_sockaddr;
  int bind_retries = 10;
  unsigned short the_port;
  /* IPPORT_RESERVED */
  unsigned short min_port = 1024;
  unsigned short max_port = 65535;
  int is_ipv6 = vsf_sysutil_sockaddr_is_ipv6(p_sess->p_local_addr);
  if (p_sess->pasv_listen_fd != -1)
  {
    die("listed fd already active");
  }

  if (tunable_pasv_min_port > min_port && tunable_pasv_min_port <= max_port)
  {
    min_port = tunable_pasv_min_port;
  }
  if (tunable_pasv_max_port >= min_port && tunable_pasv_max_port < max_port)
  {
    max_port = tunable_pasv_max_port;
  }

  while (--bind_retries)
  {
    int retval;
    double scaled_port;
    the_port = vsf_sysutil_get_random_byte();
    the_port <<= 8;
    the_port |= vsf_sysutil_get_random_byte();
    scaled_port = (double) min_port;
    scaled_port += ((double) the_port / (double) 65536) *
                   ((double) max_port - min_port + 1);
    the_port = (unsigned short) scaled_port;
    if (is_ipv6)
    {
      p_sess->pasv_listen_fd = vsf_sysutil_get_ipv6_sock();
    }
    else
    {
      p_sess->pasv_listen_fd = vsf_sysutil_get_ipv4_sock();
    }
    vsf_sysutil_activate_reuseaddr(p_sess->pasv_listen_fd);
    vsf_sysutil_sockaddr_clone(&s_p_sockaddr, p_sess->p_local_addr);
    vsf_sysutil_sockaddr_set_port(s_p_sockaddr, the_port);
    retval = vsf_sysutil_bind(p_sess->pasv_listen_fd, s_p_sockaddr);
    if (!vsf_sysutil_retval_is_error(retval))
    {
      retval = vsf_sysutil_listen(p_sess->pasv_listen_fd, 1);
      if (!vsf_sysutil_retval_is_error(retval))
      {
        break;
      }
    }
    /* SELinux systems can give you an inopportune EACCES, it seems. */
    if (vsf_sysutil_get_error() == kVSFSysUtilErrADDRINUSE ||
        vsf_sysutil_get_error() == kVSFSysUtilErrACCES)
    {
      vsf_sysutil_close(p_sess->pasv_listen_fd);
      p_sess->pasv_listen_fd = -1;
      continue;
    }
    die("vsf_sysutil_bind / listen");
  }
  if (!bind_retries)
  {
    die("vsf_sysutil_bind");
  }
  return the_port;
}