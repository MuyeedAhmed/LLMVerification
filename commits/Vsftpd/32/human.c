void
vsf_sysutil_activate_linger(int fd)
{
  int retval;
  struct linger the_linger;
  vsf_sysutil_memclr(&the_linger, sizeof(the_linger));
  the_linger.l_onoff = 1;
  the_linger.l_linger = 60 * 10;
  retval = setsockopt(fd, SOL_SOCKET, SO_LINGER, &the_linger,
                      sizeof(the_linger));
  if (retval != 0)
  {
    die("setsockopt: linger");
  }
}