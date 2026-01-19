static char *xescape_trigraphs (const char *src)
{
  char *result = xmalloc (strlen (src) * 3 + 1);
  char *dest = result;

  while (*src)
    {
      if (src[0] == '?' && src[1] == '?' && src[2] == '/')
        {
          *dest++ = '?';
          *dest++ = '?';
          *dest++ = '/';
          src += 3;
        }
      else if (src[0] == '?' && src[1] == '?' && src[2] == '(')
        {
          *dest++ = '?';
          *dest++ = '?';
          *dest++ = '(';
          src += 3;
        }
      else if (src[0] == '?' && src[1] == '?' && src[2] == ')')
        {
          *dest++ = '?';
          *dest++ = '?';
          *dest++ = ')';
          src += 3;
        }
      else if (src[0] == '?' && src[1] == '?' && src[2] == '!')
        {
          *dest++ = '?';
          *dest++ = '?';
          *dest++ = '!';
          src += 3;
        }
      else if (src[0] == '?' && src[1] == '?' && src[2] == '<')
        {
          *dest++ = '?';
          *dest++ = '?';
          *dest++ = '<';
          src += 3;
        }
      else if (src[0] == '?' && src[1] == '?' && src[2] == '>')
        {
          *dest++ = '?';
          *dest++ = '?';
          *dest++ = '>';
          src += 3;
        }
      else if (src[0] == '?' && src[1] == '?' && src[2] == '=')
        {
          *dest++ = '?';
          *dest++ = '?';
          *dest++ = '=';
          src += 3;
        }
      else if (src[0] == '?' && src[1] == '?' && src[2] == '-')
        {
          *dest++ = '?';
          *dest++ = '?';
          *dest++ = '-';
          src += 3;
        }
      else if (src[0] == '?' && src[1] == '?' && src[2] == '\'')
        {
          *dest++ = '?';
          *dest++ = '?';
          *dest++ = '\'';
          src += 3;
        }
      else
        {
          *dest++ = *src++;
        }
    }

  *dest = '\0';
  return result;
}
