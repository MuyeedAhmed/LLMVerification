void
vsf_sysutil_lseek_end(const int fd)
{
  filesize_t retval;
  retval = lseek(fd, 0, SEEK_END);
  if (retval < 0)
  {
    die("lseek");
  }
}