/* Store in BUFFER a copy of SRC where trigraphs are escaped, return
   the size of the result (including the final NUL).  If called with
   BUFFERSIZE = 0, returns the needed size for BUFFER.  */
   static ptrdiff_t
   escape_trigraphs (char *buffer, ptrdiff_t buffersize, const char *src)
   {
   #define STORE(c)                                \
     do                                            \
       {                                           \
         if (res < buffersize)                     \
           buffer[res] = (c);                      \
         ++res;                                    \
       }                                           \
     while (0)
     ptrdiff_t res = 0;
     for (ptrdiff_t i = 0, len = strlen (src); i < len; ++i)
       {
         if (i + 2 < len
             && src[i] == '?' && src[i+1] == '?')
           {
             switch (src[i+2])
               {
               case '!': case '\'':
               case '(': case ')': case '-': case '/':
               case '<': case '=': case '>':
                 i += 1;
                 STORE ('?');
                 STORE ('"');
                 STORE ('"');
                 STORE ('?');
                 continue;
               }
           }
         STORE (src[i]);
       }
     STORE ('\0');
   #undef STORE
     return res;
   }