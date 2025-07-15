void vsf_sysutil_lseek_end(const int fd)
{
    if (lseek(fd, 0, SEEK_END) < 0)
    {
        die("lseek to end");
    }
}