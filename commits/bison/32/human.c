/* Same as xstrdup, except that trigraphs are escaped.  */
static char *
xescape_trigraphs (const char *src)
{
  ptrdiff_t bufsize = escape_trigraphs (NULL, 0, src);
  char *buf = xcharalloc (bufsize);
  escape_trigraphs (buf, bufsize, src);
  return buf;
}