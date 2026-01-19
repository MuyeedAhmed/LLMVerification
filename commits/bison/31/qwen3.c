static ptrdiff_t
escape_trigraphs (char *buffer, ptrdiff_t buffersize, const char *src)
{
  ptrdiff_t len = 0;
  const char *p = src;

  while (*p)
    {
      if (len >= buffersize - 1)
        break;

      if (p[0] == '?' && p[1] == '?' && (p[2] == '=' || p[2] == '/' || p[2] == '(' || p[2] == ')' || p[2] == '\'' || p[2] == '<' || p[2] == '>' || p[2] == '-' || p[2] == '!'))
        {
          if (len + 3 >= buffersize)
            break;

          buffer[len++] = '?';
          buffer[len++] = '?';
          buffer[len++] = p[2];
          p += 2;
        }
      else
        {
          buffer[len++] = *p;
        }

      p++;
    }

  if (len < buffersize)
    buffer[len] = '\0';

  return len + 1;
}
