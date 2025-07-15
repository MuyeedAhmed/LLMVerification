/*------------------------------.
| Abort if S is not a uniqstr.  |
`------------------------------*/

void
uniqstr_assert (char const *str)
{
  uniqstr s = hash_lookup (uniqstrs_table, str);
  if (!s || s != str)
    {
      error (0, 0,
             "not a uniqstr: %s", quotearg (str));
      abort ();
    }
}