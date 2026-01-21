void vsf_sysutil_lseek_end(const int fd)
{
  lseek(fd, 0, SEEK_END);
}
